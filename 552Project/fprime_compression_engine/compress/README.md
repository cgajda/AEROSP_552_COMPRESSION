# Compression Engine – README

This document describes the capabilities, supported formats, and valid test cases for the **Compression Engine** implemented in F´ (F Prime).  
It details the three supported compression algorithms—**Huffman**, **LZSS**, and **DCT**—and how to exercise them through the F´ GDS.

---

## Overview

The compression engine exposes the following API:

```cpp
namespace CompressionLib {

  enum class Algorithm : std::uint8_t {
    HUFFMAN = 0,
    LZSS    = 1,
    DCT     = 2
  };

  struct Result {
    std::uint32_t bytesIn;
    std::uint32_t bytesOut;
    std::int32_t  error;   // 0 = OK, <0 = lib error, >0 = system error
  };

  Result compressFile(Algorithm algo, const std::string& path);
  Result decompressFile(Algorithm algo, const std::string& path);
}
