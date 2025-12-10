#include "compress/Lib/CompressionLib/Dct.hpp"
#include <cstdint>
#include <vector>
#include <fstream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "compress/Lib/CompressionLib/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "compress/Lib/CompressionLib/stb_image_write.h"

namespace CompressionLib {

namespace {

// Helper: get file size
std::uint32_t getFileSize(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return 0;
  auto size = in.tellg();
  return static_cast<std::uint32_t>(size);
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

// Very simple PPM P6 loader (no comments supported)
bool loadPpmP6(
    const std::string& path,
    int& width,
    int& height,
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

  width  = w;
  height = h;

  std::size_t expectedBytes =
      static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
  rgb.resize(expectedBytes);

  in.read(reinterpret_cast<char*>(rgb.data()), expectedBytes);
  if (!in) {
    return false;
  }

  return true;
}

// Build an output .jpg path from the input
std::string makeJpegOutPath(const std::string& inPath) {
  // If input already has an extension, strip and add .jpg
  auto dot = inPath.find_last_of('.');
  if (dot == std::string::npos) {
    return inPath + ".jpg";
  } else {
    return inPath.substr(0, dot) + ".jpg";
  }
}

} // namespace

// ======================
// "DCT" Compressor → JPEG
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
  int w = 0, h = 0;
  std::vector<std::uint8_t> rgb;

  if (hasPpmExtension(inPath)) {
    // Native PPM P6
    if (!loadPpmP6(inPath, w, h, rgb)) {
      r.error = -2; // invalid PPM
      return r;
    }
  } else {
    // Use stb_image for PNG/JPEG/etc., always request 3 channels
    int chans = 0;
    unsigned char* data = stbi_load(inPath.c_str(), &w, &h, &chans, 3);
    if (!data) {
      r.error = -7; // unsupported / failed decode
      return r;
    }

    std::size_t count =
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
    rgb.assign(data, data + count);

    stbi_image_free(data);
  }

  if (w <= 0 || h <= 0 || rgb.empty()) {
    r.error = -2; // decode failure
    return r;
  }

  const std::string outPath = makeJpegOutPath(inPath);

  // 2) Encode as JPEG (lossy, DCT-based) with chosen quality
  // Quality in [1,100]; 75–90 is a good tradeoff
  const int quality = 85;

  int ok = stbi_write_jpg(
      outPath.c_str(),
      w,
      h,
      3,                      // 3 channels (RGB)
      rgb.data(),
      quality
  );

  if (!ok) {
    r.error = -3; // could not write output
    return r;
  }

  // 3) Output stats
  r.bytesOut = getFileSize(outPath);
  r.error    = 0;
  // If Result has an outputPath field, you can set it:
  // r.outputPath = outPath;

  return r;
}

// ======================
// Decompressor stub
// (not really needed for JPEG; viewing the file is decompression)
// ======================

Result dctDecompressFile(const std::string& inPath) {
  Result r{};
  r.error    = -9; // "no explicit decompression implemented"
  r.bytesIn  = getFileSize(inPath);
  r.bytesOut = 0;
  return r;
}

} // namespace CompressionLib
