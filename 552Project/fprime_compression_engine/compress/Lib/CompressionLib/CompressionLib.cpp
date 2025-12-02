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
      r.error = -99;
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
    case Algorithm::DCT:
      // path should be the .dct file
      return dctDecompressFile(path);
    default: {
      Result r{};
      r.error = -99;
      return r;
    }
  }
}

Result compressFolder(Algorithm algo, const std::string& folder) {
  (void)algo;
  (void)folder;
  Result r{};
  r.error = -1; // not implemented
  return r;
}

} // namespace CompressionLib
