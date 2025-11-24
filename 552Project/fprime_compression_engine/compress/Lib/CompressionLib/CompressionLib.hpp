#ifndef COMPRESSION_LIB_COMPRESSION_LIB_HPP
#define COMPRESSION_LIB_COMPRESSION_LIB_HPP

#include <cstdint>
#include <string>

namespace CompressionLib {

  // Must match your FPP values: 0=HUFFMAN, 1=LZSS, 2=DCT
  enum class Algorithm : std::uint8_t {
    HUFFMAN = 0,
    LZSS    = 1,
    DCT     = 2
  };

  struct Result {
    std::uint32_t bytesIn  = 0;
    std::uint32_t bytesOut = 0;
    std::int32_t  error    = 0;   // 0 = OK, <0 = lib error, >0 = system error
  };

  // Compress a single file on disk. Returns Result with sizes.
  Result compressFile(Algorithm algo, const std::string& path);

  // Compress all files in a folder (for now: stub)
  Result compressFolder(Algorithm algo, const std::string& folder);

} // namespace CompressionLib

#endif
