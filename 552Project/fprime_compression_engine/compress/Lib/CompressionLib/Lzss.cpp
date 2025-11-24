#include "compress/Lib/CompressionLib/Lzss.hpp"
#include <fstream>

namespace CompressionLib {

  Result lzssCompressFile(const std::string& inPath) {
    Result r{};
    std::ifstream in(inPath, std::ios::binary | std::ios::ate);
    if (!in) {
      r.error = -1;
      return r;
    }
    auto size = in.tellg();
    in.close();

    // stub: just copy
    std::ifstream src(inPath, std::ios::binary);
    std::ofstream dst(inPath + ".lzss", std::ios::binary);
    dst << src.rdbuf();
    src.close();
    dst.close();

    r.bytesIn  = static_cast<std::uint32_t>(size);
    r.bytesOut = static_cast<std::uint32_t>(size);
    r.error    = 0;
    return r;
  }

} // namespace CompressionLib
