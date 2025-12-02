// CompressionLib.cpp
#include "compress/Lib/CompressionLib/CompressionLib.hpp"

#include "compress/Lib/CompressionLib/Huffman.hpp"
#include "compress/Lib/CompressionLib/Lzss.hpp"
#include "compress/Lib/CompressionLib/Dct.hpp"

namespace CompressionLib {

Result compressFile(Algorithm algo, const std::string& path) {
  switch (algo) {
    case Algorithm::HUFFMAN:
      return huffmanCompressFile(path);
    case Algorithm::LZSS:
      return lzssCompressFile(path);
    case Algorithm::DCT:
      return dctCompressFile(path);
    default: {
      Result r{};
      r.error = -99; // unknown algorithm
      return r;
    }
  }
}

Result decompressFile(Algorithm algo, const std::string& path) {
  switch (algo) {
    case Algorithm::HUFFMAN:
      return huffmanDecompressFile(path);
    case Algorithm::LZSS:
      return lzssDecompressFile(path);
    case Algorithm::DCT: {
      // DCT decompression not implemented yet
      Result r{};
      r.error = -10; // "DCT decompression not implemented"
      return r;
    }
    default: {
      Result r{};
      r.error = -99;
      return r;
    }
  }
}

// If you already have a stub for compressFolder, keep it as-is for now
Result compressFolder(Algorithm algo, const std::string& folder) {
  // TODO: real folder recursion; for now just stub
  (void)algo;
  (void)folder;
  Result r{};
  r.error = -1; // not implemented
  return r;
}

} // namespace CompressionLib
