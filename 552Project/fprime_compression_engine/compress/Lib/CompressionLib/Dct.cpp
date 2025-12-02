#include "compress/Lib/CompressionLib/Dct.hpp"

#include <cstdint>
#include <vector>
#include <fstream>
#include <cmath>
#include <cstring>   // std::memcpy
#include <algorithm> // std::min, std::max

namespace CompressionLib {

namespace {

// Simple helper to get file size
std::uint32_t getFileSize(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return 0;
  auto size = in.tellg();
  return static_cast<std::uint32_t>(size);
}

// Very simple PPM P6 loader (no comments supported for now)
bool loadPpmP6(
    const std::string& path,
    std::uint16_t& width,
    std::uint16_t& height,
    std::vector<std::uint8_t>& rgb // out: size = width*height*3
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

  // Skip single whitespace character after maxval
  in.get();

  width  = static_cast<std::uint16_t>(w);
  height = static_cast<std::uint16_t>(h);

  std::size_t expectedBytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
  rgb.resize(expectedBytes);

  in.read(reinterpret_cast<char*>(rgb.data()), expectedBytes);
  if (!in) {
    return false;
  }

  return true;
}

// Convert RGB (interleaved) to grayscale luminance [0, 255]
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
    // Standard Rec.601 luma approx
    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    grayOut[i] = y;
  }
}

// Pad image to multiple of 8 in both dimensions
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

// 8x8 DCT basis constants
inline double alpha(int k) {
  return (k == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
}

// Compute 2D DCT for a single 8x8 block
void dct8x8(const float in[64], float out[64]) {
  const double pi = std::acos(-1.0);
  for (int v = 0; v < 8; ++v) {
    for (int u = 0; u < 8; ++u) {
      double sum = 0.0;
      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          double fxy = static_cast<double>(in[y * 8 + x]) - 128.0; // center at 0
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

// Basic JPEG-like luminance quantization matrix
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

// Quantize 8x8 block of DCT coefficients -> int16
void quantize8x8(const float in[64], std::int16_t out[64]) {
  for (int i = 0; i < 64; ++i) {
    float q = static_cast<float>(kLumaQuant[i]);
    float val = in[i] / q;
    // Round to nearest integer
    int qval = static_cast<int>(std::round(val));
    // Clamp to int16 range just in case
    qval = std::max(-32768, std::min(32767, qval));
    out[i] = static_cast<std::int16_t>(qval);
  }
}

} // namespace

Result dctCompressFile(const std::string& inPath) {
  Result r{};
  r.error    = 0;
  r.bytesIn  = 0;
  r.bytesOut = 0;

  // 1) Get input size (for stats)
  r.bytesIn = getFileSize(inPath);
  if (r.bytesIn == 0) {
    r.error = -1; // could not open input
    return r;
  }

  // 2) Load PPM
  std::uint16_t width = 0, height = 0;
  std::vector<std::uint8_t> rgb;
  if (!loadPpmP6(inPath, width, height, rgb)) {
    r.error = -2; // invalid or unsupported input format
    return r;
  }

  // 3) Convert to grayscale
  std::vector<float> gray;
  rgbToGrayscale(rgb, width, height, gray);

  // 4) Pad to multiples of 8
  std::uint16_t paddedW = 0, paddedH = 0;
  std::vector<float> padded;
  padToBlockSize(gray, width, height, paddedW, paddedH, padded);

  const int block = 8;
  const int blocksX = paddedW / block;
  const int blocksY = paddedH / block;
  const std::size_t numBlocks = static_cast<std::size_t>(blocksX) * blocksY;

  // 5) Open output file
  std::string outPath = inPath + ".dct";
  std::ofstream out(outPath, std::ios::binary);
  if (!out) {
    r.error = -3; // could not open output
    return r;
  }

  // 6) Write header
  char magic[4] = { 'D', 'C', 'T', '1' };
  out.write(magic, 4);

  // Store original width/height, not padded
  std::uint16_t w_le = width;
  std::uint16_t h_le = height;
  out.write(reinterpret_cast<const char*>(&w_le), sizeof(w_le));
  out.write(reinterpret_cast<const char*>(&h_le), sizeof(h_le));

  std::uint8_t channels = 1; // grayscale
  out.write(reinterpret_cast<const char*>(&channels), sizeof(channels));

  // 7) Process each 8x8 block
  float blockIn[64];
  float blockDct[64];
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

      // Write quantized coefficients for this block
      out.write(reinterpret_cast<const char*>(blockQ), sizeof(blockQ));
    }
  }

  out.close();

  // 8) Fill stats
  r.bytesOut = static_cast<std::uint32_t>(getFileSize(outPath));
  r.error    = 0;
  return r;
}

} // namespace CompressionLib
