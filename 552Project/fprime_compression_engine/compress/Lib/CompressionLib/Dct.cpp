#include "compress/Lib/CompressionLib/Dct.hpp"

#include <cstdint>
#include <vector>
#include <fstream>
#include <cmath>
#include <cstring>   // std::strncmp
#include <algorithm> // std::min, std::max
#include <cctype>    // std::tolower
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "compress/Lib/CompressionLib/stb_image.h"

namespace CompressionLib {

namespace {

// Helper: get file size
std::uint32_t getFileSize(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return 0;
  auto size = in.tellg();
  return static_cast<std::uint32_t>(size);
}

// Very simple PPM P6 loader (no comments supported)
bool loadPpmP6(
    const std::string& path,
    std::uint16_t& width,
    std::uint16_t& height,
    std::vector<std::uint8_t>& rgb // out: width * height * 3
) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;

  std::string magic;
  in >> magic;
  if (magic != "P6") {
    return false;
  }

  int w = 0, h = 0, maxval = 0;
  in >> w >> h >> maxval;
  if (!in || w <= 0 || h <= 0 || maxval <= 0 || maxval > 255) {
    return false;
  }

  // Skip single whitespace after maxval
  in.get();

  width  = static_cast<std::uint16_t>(w);
  height = static_cast<std::uint16_t>(h);

  std::size_t expectedBytes =
      static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
  rgb.resize(expectedBytes);

  in.read(reinterpret_cast<char*>(rgb.data()), expectedBytes);
  if (!in) {
    return false;
  }

  return true;
}

// RGB (interleaved) → grayscale luminance [0,255] stored as float
void rgbToGrayscale(
    const std::vector<std::uint8_t>& rgb,
    std::uint16_t width,
    std::uint16_t height,
    std::vector<float>& grayOut
) {
  grayOut.resize(static_cast<std::size_t>(width) * height);

  for (std::size_t i = 0, p = 0; i < grayOut.size(); ++i, p += 3) {
    float r = static_cast<float>(rgb[p + 0]);
    float g = static_cast<float>(rgb[p + 1]);
    float b = static_cast<float>(rgb[p + 2]);
    // Rec.601 luma
    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    grayOut[i] = y;
  }
}

// Pad to multiples of 8
void padToBlockSize(
    const std::vector<float>& in,
    std::uint16_t width,
    std::uint16_t height,
    std::uint16_t& paddedW,
    std::uint16_t& paddedH,
    std::vector<float>& padded
) {
  const int block = 8;
  paddedW = static_cast<std::uint16_t>((width  + block - 1) / block * block);
  paddedH = static_cast<std::uint16_t>((height + block - 1) / block * block);

  padded.assign(static_cast<std::size_t>(paddedW) * paddedH, 0.0f);

  for (std::uint16_t y = 0; y < height; ++y) {
    for (std::uint16_t x = 0; x < width; ++x) {
      padded[static_cast<std::size_t>(y) * paddedW + x] =
          in[static_cast<std::size_t>(y) * width + x];
    }
  }
}

// DCT basis scaling
inline double alpha(int k) {
  return (k == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
}

// 2D 8×8 forward DCT on a block
void dct8x8(const float in[64], float out[64]) {
  const double pi = std::acos(-1.0);
  for (int v = 0; v < 8; ++v) {
    for (int u = 0; u < 8; ++u) {
      double sum = 0.0;
      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          double fxy = static_cast<double>(in[y * 8 + x]) - 128.0; // center around 0
          double cosx = std::cos(((2.0 * x + 1.0) * u * pi) / 16.0);
          double cosy = std::cos(((2.0 * y + 1.0) * v * pi) / 16.0);
          sum += fxy * cosx * cosy;
        }
      }
      double cu = alpha(u);
      double cv = alpha(v);
      out[v * 8 + u] = static_cast<float>(0.25 * cu * cv * sum);
    }
  }
}

// JPEG-like luminance quantization matrix
static const int kLumaQuant[64] = {
   16, 11, 10, 16,  24,  40,  51,  61,
   12, 12, 14, 19,  26,  58,  60,  55,
   14, 13, 16, 24,  40,  57,  69,  56,
   14, 17, 22, 29,  51,  87,  80,  62,
   18, 22, 37, 56,  68, 109, 103,  77,
   24, 35, 55, 64,  81, 104, 113,  92,
   49, 64, 78, 87, 103, 121, 120, 101,
   72, 92, 95, 98, 112, 100, 103,  99
};

// Quantize 8×8 DCT block -> int16
void quantize8x8(const float in[64], std::int16_t out[64]) {
  for (int i = 0; i < 64; ++i) {
    float q = static_cast<float>(kLumaQuant[i]);
    float val = in[i] / q;
    int qval = static_cast<int>(std::round(val));
    // clamp to int16 range
    qval = std::max(-32768, std::min(32767, qval));
    out[i] = static_cast<std::int16_t>(qval);
  }
}

// Dequantize 8×8 -> float DCT coefficients
void dequantize8x8(const std::int16_t in[64], float out[64]) {
  for (int i = 0; i < 64; ++i) {
    out[i] = static_cast<float>(in[i]) * static_cast<float>(kLumaQuant[i]);
  }
}

// 2D 8×8 inverse DCT (matches dct8x8 convention)
void idct8x8(const float in[64], float out[64]) {
  const double pi = std::acos(-1.0);

  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      double sum = 0.0;
      for (int v = 0; v < 8; ++v) {
        for (int u = 0; u < 8; ++u) {
          double cu = alpha(u);
          double cv = alpha(v);
          double Fuv = static_cast<double>(in[v * 8 + u]);
          double cosx = std::cos(((2.0 * x + 1.0) * u * pi) / 16.0);
          double cosy = std::cos(((2.0 * y + 1.0) * v * pi) / 16.0);
          sum += cu * cv * Fuv * cosx * cosy;
        }
      }
      // Inverse scaling (forward had 0.25 * alpha*alpha*sum)
      double fxy = 0.25 * sum + 128.0; // re-add bias
      out[y * 8 + x] = static_cast<float>(fxy);
    }
  }
}

// Simple extension check for ".ppm" (case-insensitive)
bool hasPpmExtension(const std::string& path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return false;
  std::string ext = path.substr(dot + 1);
  for (char& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return ext == "ppm";
}

} // namespace

// ======================
// Compressor
// ======================

Result dctCompressFile(const std::string& inPath) {
  Result r{};
  r.error    = 0;
  r.bytesIn  = 0;
  r.bytesOut = 0;

  // Input size (for stats only)
  r.bytesIn = getFileSize(inPath);
  if (r.bytesIn == 0) {
    r.error = -1; // could not open input
    return r;
  }

  // 1) Decode input into RGB buffer + dimensions
  std::uint16_t width = 0, height = 0;
  std::vector<std::uint8_t> rgb;

  const bool isPpm = hasPpmExtension(inPath);

  if (isPpm) {
    // Native PPM path
    if (!loadPpmP6(inPath, width, height, rgb)) {
      r.error = -2; // invalid PPM
      return r;
    }
  } else {
    // Non-PPM: use stb_image to decode into RGB
    int w = 0, h = 0, chans = 0;
    unsigned char* data = stbi_load(inPath.c_str(), &w, &h, &chans, 3);
    if (!data) {
      r.error = -7; // unsupported / failed decode
      return r;
    }

    width  = static_cast<std::uint16_t>(w);
    height = static_cast<std::uint16_t>(h);

    std::size_t count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
    rgb.assign(data, data + count);

    stbi_image_free(data);
  }

  // 2) RGB -> grayscale
  std::vector<float> gray;
  rgbToGrayscale(rgb, width, height, gray);

  // 3) Pad to multiples of 8
  std::uint16_t paddedW = 0, paddedH = 0;
  std::vector<float> padded;
  padToBlockSize(gray, width, height, paddedW, paddedH, padded);

  const int block   = 8;
  const int blocksX = paddedW / block;
  const int blocksY = paddedH / block;

  // 4) Open output file
  const std::string outPath = inPath + ".dct";
  std::ofstream out(outPath, std::ios::binary);
  if (!out) {
    r.error = -3; // could not open output
    return r;
  }

  // 5) Header
  char magic[4] = { 'D', 'C', 'T', '1' };
  out.write(magic, 4);

  // Original width/height (not padded)
  std::uint16_t w_le = width;
  std::uint16_t h_le = height;
  out.write(reinterpret_cast<const char*>(&w_le), sizeof(w_le));
  out.write(reinterpret_cast<const char*>(&h_le), sizeof(h_le));

  // channels (1 = grayscale)
  std::uint8_t channels = 1;
  out.write(reinterpret_cast<const char*>(&channels), sizeof(channels));

  // 6) Process each 8x8 block
  float       blockIn[64];
  float       blockDct[64];
  std::int16_t blockQ[64];

  for (int by = 0; by < blocksY; ++by) {
    for (int bx = 0; bx < blocksX; ++bx) {

      // Extract 8x8 block
      for (int y = 0; y < block; ++y) {
        for (int x = 0; x < block; ++x) {
          std::uint16_t px = static_cast<std::uint16_t>(bx * block + x);
          std::uint16_t py = static_cast<std::uint16_t>(by * block + y);
          blockIn[y * block + x] =
              padded[static_cast<std::size_t>(py) * paddedW + px];
        }
      }

      // DCT
      dct8x8(blockIn, blockDct);

      // Quantize
      quantize8x8(blockDct, blockQ);

      // Write this block's coefficients
      out.write(reinterpret_cast<const char*>(blockQ), sizeof(blockQ));
    }
  }

  out.close();

  // 7) Output stats
  r.bytesOut = getFileSize(outPath);
  r.error    = 0;
  return r;
}

// ======================
// Decompressor
// ======================

Result dctDecompressFile(const std::string& inPath) {
  Result r{};
  r.error    = 0;
  r.bytesIn  = 0;
  r.bytesOut = 0;

  // Input .dct size
  r.bytesIn = getFileSize(inPath);
  if (r.bytesIn == 0) {
    r.error = -1; // could not open input
    return r;
  }

  std::ifstream in(inPath, std::ios::binary);
  if (!in) {
    r.error = -1;
    return r;
  }

  // 1) Read header
  char magic[4] = {0};
  in.read(magic, 4);
  if (!in || std::strncmp(magic, "DCT1", 4) != 0) {
    r.error = -4; // invalid magic / not a DCT1 file
    return r;
  }

  std::uint16_t width  = 0;
  std::uint16_t height = 0;
  in.read(reinterpret_cast<char*>(&width),  sizeof(width));
  in.read(reinterpret_cast<char*>(&height), sizeof(height));
  if (!in || width == 0 || height == 0) {
    r.error = -4; // invalid header
    return r;
  }

  std::uint8_t channels = 0;
  in.read(reinterpret_cast<char*>(&channels), sizeof(channels));
  if (!in || channels != 1) {
    r.error = -5; // unsupported channels (we only handle grayscale)
    return r;
  }

  // 2) Reconstruct padded dimensions and block counts
  const int block = 8;
  std::uint16_t paddedW =
      static_cast<std::uint16_t>((width  + block - 1) / block * block);
  std::uint16_t paddedH =
      static_cast<std::uint16_t>((height + block - 1) / block * block);

  const int blocksX = paddedW / block;
  const int blocksY = paddedH / block;

  // 3) Buffer for reconstructed padded grayscale image
  std::vector<float> padded(static_cast<std::size_t>(paddedW) * paddedH, 0.0f);

  // 4) For each block: read Q coefficients, dequantize, IDCT, write block
  std::int16_t blockQ[64];
  float        blockF[64];
  float        blockSpatial[64];

  for (int by = 0; by < blocksY; ++by) {
    for (int bx = 0; bx < blocksX; ++bx) {

      in.read(reinterpret_cast<char*>(blockQ), sizeof(blockQ));
      if (!in) {
        r.error = -6; // truncated or invalid coefficient data
        return r;
      }

      // Dequantize
      dequantize8x8(blockQ, blockF);

      // IDCT
      idct8x8(blockF, blockSpatial);

      // Store into padded image
      for (int y = 0; y < block; ++y) {
        for (int x = 0; x < block; ++x) {
          std::uint16_t px = static_cast<std::uint16_t>(bx * block + x);
          std::uint16_t py = static_cast<std::uint16_t>(by * block + y);
          padded[static_cast<std::size_t>(py) * paddedW + px] =
              blockSpatial[y * block + x];
        }
      }
    }
  }

  in.close();

  // 5) Crop back to original width/height and clamp to [0,255]
  std::vector<std::uint8_t> gray(static_cast<std::size_t>(width) * height);
  for (std::uint16_t y = 0; y < height; ++y) {
    for (std::uint16_t x = 0; x < width; ++x) {
      float val = padded[static_cast<std::size_t>(y) * paddedW + x];
      // clamp and round
      val = std::max(0.0f, std::min(255.0f, val));
      std::uint8_t p = static_cast<std::uint8_t>(std::round(val));
      gray[static_cast<std::size_t>(y) * width + x] = p;
    }
  }

  // 6) Write out PGM (P5) grayscale image
  const std::string outPath = inPath + ".pgm";
  std::ofstream out(outPath, std::ios::binary);
  if (!out) {
    r.error = -3; // could not open output
    return r;
  }

  out << "P5\n" << width << " " << height << "\n255\n";
  out.write(reinterpret_cast<const char*>(gray.data()), gray.size());
  out.close();

  r.bytesOut = getFileSize(outPath);
  r.error    = 0;
  return r;
}

} // namespace CompressionLib
