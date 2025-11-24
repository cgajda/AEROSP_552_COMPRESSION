#include "compress/Lib/CompressionLib/Dct.hpp"
#include <fstream>

namespace CompressionLib {

  Result dctCompressFile(const std::string& inPath) {
    Result r{};
    // for now: just write a marker file
    std::ifstream in(inPath, std::ios::binary | std::ios::ate);
    if (!in) {
      r.error = -1;
      return r;
    }
    auto size = in.tellg();
    in.close();

    std::ofstream out(inPath + ".dct", std::ios::binary);
    if (!out) {
      r.error = -2;
      return r;
    }
    out.write("DCT0", 4);  // header marker
    out.close();

    r.bytesIn  = static_cast<std::uint32_t>(size);
    r.bytesOut = 4;
    r.error    = 0;
    return r;
  }

} // namespace CompressionLib
