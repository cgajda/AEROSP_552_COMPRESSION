#ifndef COMPRESSION_LIB_DCT_HPP
#define COMPRESSION_LIB_DCT_HPP

#include <string>
#include "compress/Lib/CompressionLib/CompressionLib.hpp"

namespace CompressionLib {

  /**
   * Lossy DCT-based image compressor.
   *
   * Expected input:
   *  - 8-bit binary PPM ("P6") file
   *  - 3 channels (RGB), maxval <= 255
   *
   * Output:
   *  - Custom .dct file:
   *      [char magic[4]]      = "DCT1"
   *      [uint16_t width]
   *      [uint16_t height]
   *      [uint8_t channels]   = 1 (grayscale)
   *      [int16_t coeffs[]]   = quantized 8x8 DCT coefficients in raster 8x8 order
   *
   * Returns:
   *  - Result.bytesIn  = size of input image
   *  - Result.bytesOut = size of .dct file
   *  - Result.error    = 0 on success, <0 on error
   */
  Result dctCompressFile(const std::string& inPath);

    /**
   * DCT-based decompressor.
   *
   * Expected input (.dct):
   *  - Format as written by dctCompressFile above.
   *
   * Output:
   *  - Binary PGM ("P5") grayscale image at "<inPath>.pgm"
   *    with original width/height, 8-bit pixels.
   *
   * Result:
   *  - bytesIn  = size of .dct file
   *  - bytesOut = size of .pgm file
   *  - error    = 0 on success, <0 on error
   */
  Result dctDecompressFile(const std::string& inPath);
  
} // namespace CompressionLib

#endif
