#include "compress/Lib/CompressionLib/CompressionLib.hpp"
#include "compress/Lib/CompressionLib/Huffman.hpp"
#include "compress/Lib/CompressionLib/Lzss.hpp"
#include "compress/Lib/CompressionLib/Dct.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

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

  Result compressFolder(Algorithm algo, const std::string& folder) {
    Result total{};
    total.error = 0;

    DIR* dir = opendir(folder.c_str());
    if (!dir) {
      total.error = -10;  // couldn’t open dir
      return total;
    }

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
      // skip . and ..
      if (ent->d_name[0] == '.') {
        continue;
      }

      std::string path = folder;
      if (!path.empty() && path.back() != '/') {
        path += '/';
      }
      path += ent->d_name;

      // check if regular file
      struct stat st;
      if (stat(path.c_str(), &st) != 0) {
        // can’t stat: skip, but record error
        total.error = errno;
        continue;
      }
      if (!S_ISREG(st.st_mode)) {
        // not a regular file: skip
        continue;
      }

      // compress this file
      Result r = compressFile(algo, path);
      total.bytesIn  += r.bytesIn;
      total.bytesOut += r.bytesOut;
      if (r.error != 0) {
        total.error = r.error;   // remember last error
      }
    }

    closedir(dir);
    return total;
  }

} // namespace CompressionLib
