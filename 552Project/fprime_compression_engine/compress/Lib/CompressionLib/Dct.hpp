#ifndef COMPRESSION_LIB_DCT_HPP
#define COMPRESSION_LIB_DCT_HPP

#include <string>
#include "compress/Lib/CompressionLib/CompressionLib.hpp"

namespace CompressionLib {

  /**
   * Lossy DCT-based image compressor.
   *
   * Supported input formats:
   *  - Native: 8-bit binary PPM ("P6") RGB
   *  - Via stb_image conversion (decoded to RGB in-memory):
   *      PNG, JPEG, BMP, TGA, PSD, HDR, PIC, PNM, QOI, etc.
   *
   * Output (.dct) format:
   *  - char    magic[4]   = "DCT1"
   *  - uint16  width      = original width (before padding)
   *  - uint16  height     = original height (before padding)
   *  - uint8   channels   = 1 (grayscale)
   *  - int16   coeffs[]   = quantized 8×8 DCT coefficients, block-raster order
   *
   * Result:
   *  - bytesIn  = size of original input file (PNG/JPG/PPM/etc.)
   *  - bytesOut = size of .dct file
   *  - error    = 0 on success
   *              -1: could not open input or size 0
   *              -2: invalid PPM file when extension is .ppm
   *              -3: could not open output file
   *              -7: stb_image failed to decode non-PPM input
   */
  Result dctCompressFile(const std::string& inPath);

  /**
   * DCT-based decompressor.
   *
   * Expected input:
   *  - .dct file created by dctCompressFile (magic "DCT1").
   *
   * Output:
   *  - Binary PGM ("P5") grayscale image at "<inPath>.pgm"
   *    with original width/height, 8-bit pixels.
   *
   * Result:
   *  - bytesIn  = size of input .dct file
   *  - bytesOut = size of output .pgm file
   *  - error    = 0 on success
   *              -1: could not open input or size 0
   *              -3: could not open output file
   *              -4: invalid header / magic / dimensions
   *              -5: unsupported channels (must be 1)
   *              -6: truncated coefficient data
   */
  Result dctDecompressFile(const std::string& inPath);

} // namespace CompressionLib

#endif
