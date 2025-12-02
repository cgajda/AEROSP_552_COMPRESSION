#include "compress/Lib/CompressionLib/Lzss.hpp"

#include <cstdint>
#include <fstream>
#include <vector>

namespace CompressionLib {

namespace {

struct Params {
  std::size_t windowSize  = 4096; // dictionary size
  std::size_t lookahead   = 18;   // max match length
  std::size_t minMatch    = 3;    // minimum useful match length
};

struct Match {
  std::size_t offset = 0;
  std::size_t length = 0;
};

// Naive search for best match in previous window
Match findBestMatch(const std::vector<std::uint8_t>& in,
                    std::size_t pos,
                    const Params& params) {
  Match best;
  if (pos == 0) {
    return best;
  }

  const std::size_t n = in.size();
  const std::size_t windowStart =
      (pos > params.windowSize) ? (pos - params.windowSize) : 0;
  const std::size_t maxLen =
      (pos + params.lookahead <= n) ? params.lookahead : (n - pos);

  for (std::size_t j = windowStart; j < pos; ++j) {
    std::size_t k = 0;
    while (k < maxLen && in[j + k] == in[pos + k]) {
      ++k;
    }
    if (k > best.length) {
      best.length = k;
      best.offset = pos - j;
      if (best.length == maxLen) {
        break; // can't do better than maxLen
      }
    }
  }

  // Enforce minimum match length: otherwise treat as no match
  if (best.length < params.minMatch) {
    best.length = 0;
    best.offset = 0;
  }

  return best;
}

// Core LZSS encoder: in -> out, returns true on success
bool lzssCompressBuffer(const std::vector<std::uint8_t>& in,
                        std::vector<std::uint8_t>& out,
                        const Params& params) {
  out.clear();
  const std::size_t n = in.size();
  std::size_t pos = 0;

  while (pos < n) {
    // Reserve flag byte (will fill after processing up to 8 tokens)
    std::size_t flagIndex = out.size();
    out.push_back(0);
    std::uint8_t flags = 0;

    for (int bit = 0; bit < 8 && pos < n; ++bit) {
      Match best = findBestMatch(in, pos, params);

      if (best.length > 0) {
        // Match token: flag bit = 1
        flags |= static_cast<std::uint8_t>(1u << bit);

        // offset as 16 bits (little-endian)
        std::uint16_t off = static_cast<std::uint16_t>(best.offset);
        std::uint8_t offLo = static_cast<std::uint8_t>(off & 0xFFu);
        std::uint8_t offHi = static_cast<std::uint8_t>((off >> 8) & 0xFFu);

        // length as 1 byte (assumes lookahead <= 255)
        std::uint8_t lenByte = static_cast<std::uint8_t>(best.length);

        out.push_back(offLo);
        out.push_back(offHi);
        out.push_back(lenByte);

        pos += best.length;
      } else {
        // Literal token: flag bit = 0 (already 0)
        out.push_back(in[pos]);
        ++pos;
      }
    }

    // Now we know the flags for this group of up to 8 tokens
    out[flagIndex] = flags;
  }

  return true;
}

// Core LZSS decoder: in -> out, returns true on success
bool lzssDecompressBuffer(const std::vector<std::uint8_t>& in,
                          std::vector<std::uint8_t>& out) {
  out.clear();
  const std::size_t n = in.size();
  std::size_t pos = 0;

  while (pos < n) {
    // Need at least one byte for flags
    std::uint8_t flags = in[pos++];
    for (int bit = 0; bit < 8 && pos < n; ++bit) {
      bool isMatch = ((flags >> bit) & 0x1u) != 0;

      if (isMatch) {
        // We need 3 bytes: offLo, offHi, len
        if (pos + 2 >= n) {
          // Not enough bytes for a complete match token
          return false;
        }
        std::uint8_t offLo = in[pos++];
        std::uint8_t offHi = in[pos++];
        std::uint8_t lenByte = in[pos++];

        std::uint16_t off = static_cast<std::uint16_t>(
            offLo | (static_cast<std::uint16_t>(offHi) << 8));
        std::size_t length = lenByte;

        if (off == 0 || length == 0 || off > out.size()) {
          // Invalid reference
          return false;
        }

        std::size_t start = out.size() - off;
        for (std::size_t k = 0; k < length; ++k) {
          // Overlap-safe: out grows as we push_back, but
          // index start+k always < current out.size()
          std::uint8_t b = out[start + k];
          out.push_back(b);
        }
      } else {
        // Literal: need one byte
        if (pos >= n) {
          // Expecting literal but at end of input
          return false;
        }
        out.push_back(in[pos++]);
      }
    }
  }

  return true;
}

// Helper to choose an output path from an input .lzss file
std::string deriveOutputPath(const std::string& inPath) {
  const std::string ext = ".lzss";
  if (inPath.size() >= ext.size() &&
      inPath.compare(inPath.size() - ext.size(), ext.size(), ext) == 0) {
    // Strip ".lzss"
    return inPath.substr(0, inPath.size() - ext.size());
  }
  // Fallback
  return inPath + ".orig";
}

} // namespace

// -------------------- Public API: compress file --------------------

Result lzssCompressFile(const std::string& inPath) {
  Result r{};

  // Open once to get size (for bytesIn)
  std::ifstream sizeStream(inPath, std::ios::binary | std::ios::ate);
  if (!sizeStream) {
    r.error = -1; // file open error
    return r;
  }
  auto size = sizeStream.tellg();
  sizeStream.close();

  if (size < 0) {
    r.error = -1;
    return r;
  }

  r.bytesIn = static_cast<std::uint32_t>(size);

  // Read entire file into memory
  std::ifstream in(inPath, std::ios::binary);
  if (!in) {
    r.error = -1;
    return r;
  }

  std::vector<std::uint8_t> input;
  input.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
  in.close();

  // Compress
  std::vector<std::uint8_t> output;
  Params params;
  params.windowSize = 4096;
  params.lookahead  = 18;
  params.minMatch   = 3;

  bool ok = lzssCompressBuffer(input, output, params);
  if (!ok) {
    r.error = -3; // internal LZSS error
    return r;
  }

  // Write to <inPath>.lzss
  const std::string outPath = inPath + ".lzss";
  std::ofstream dst(outPath, std::ios::binary | std::ios::trunc);
  if (!dst) {
    r.error = -2; // write error
    return r;
  }
  if (!output.empty()) {
    dst.write(reinterpret_cast<const char*>(output.data()),
              static_cast<std::streamsize>(output.size()));
  }
  dst.close();

  r.bytesOut = static_cast<std::uint32_t>(output.size());
  r.error    = 0;
  return r;
}

// -------------------- Public API: decompress file --------------------

Result lzssDecompressFile(const std::string& inPath) {
  Result r{};

  // Read compressed file
  std::ifstream in(inPath, std::ios::binary | std::ios::ate);
  if (!in) {
    r.error = -1; // file open error
    return r;
  }
  auto size = in.tellg();
  if (size < 0) {
    r.error = -1;
    return r;
  }
  r.bytesIn = static_cast<std::uint32_t>(size);
  in.seekg(0, std::ios::beg);

  std::vector<std::uint8_t> input;
  input.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
  in.close();

  // Decompress
  std::vector<std::uint8_t> output;
  bool ok = lzssDecompressBuffer(input, output);
  if (!ok) {
    r.error = -3; // LZSS decode error (bad format, etc.)
    return r;
  }

  // Choose output path
  const std::string outPath = deriveOutputPath(inPath);
  std::ofstream dst(outPath, std::ios::binary | std::ios::trunc);
  if (!dst) {
    r.error = -2; // write error
    return r;
  }
  if (!output.empty()) {
    dst.write(reinterpret_cast<const char*>(output.data()),
              static_cast<std::streamsize>(output.size()));
  }
  dst.close();

  r.bytesOut = static_cast<std::uint32_t>(output.size());
  r.error    = 0;
  return r;
}

} // namespace CompressionLib
