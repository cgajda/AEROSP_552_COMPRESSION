#ifndef COMPRESSION_LIB_HUFFMAN_HPP
#define COMPRESSION_LIB_HUFFMAN_HPP

#include <cstdint>
#include <string>
#include "compress/Lib/CompressionLib/CompressionLib.hpp"

namespace CompressionLib {

  // returns Result with .error = 0 on success
  Result huffmanCompressFile(const std::string& inPath);

} // namespace CompressionLib

#endif
