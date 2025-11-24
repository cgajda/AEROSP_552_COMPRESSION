#ifndef COMPRESSION_LIB_LZSS_HPP
#define COMPRESSION_LIB_LZSS_HPP

#include <string>
#include "compress/Lib/CompressionLib/CompressionLib.hpp"

namespace CompressionLib {

  Result lzssCompressFile(const std::string& inPath);

} // namespace CompressionLib

#endif
