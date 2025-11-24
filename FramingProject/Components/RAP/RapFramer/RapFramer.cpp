// ======================================================================
// \title  RapFramer.cpp
// \brief  Framer component for the RAP packetization protocol placeholder. Use fprime-util impl
// ======================================================================
#include "Components/RAP/RapFramer/RapFramer.hpp"
#include "Fw/Logger/Logger.hpp"

namespace Drv {

  // Constructor
  RapFramer::RapFramer(const char* const compName) :
      RapFramerComponentBase(compName) {
  }

  // Destructor
  RapFramer::~RapFramer() {}

  // ================================================================
  // Handlers
  // ================================================================

  void RapFramer::dataIn_handler(
      FwIndexType portNum,
      Fw::Buffer& data,
      const ComCfg::FrameContext& context
  ) {
      // Allocate a new buffer for the framed packet
      Fw::Buffer framedBuffer = this->allocate_out(0, data.getSize() + 16); // +header/footer (rough estimate)

      this->buildRapFrame(data, framedBuffer);

      // Debug: emit event
      this->log_ACTIVITY_HI_FrameBuilt(framedBuffer.getSize());

      // Send framed data to the next stage
      this->framedOut_out(0, framedBuffer, context);

      // Return the original buffer if owned by a manager
      this->dataReturnIn_handler(0, data, context);
  }

  void RapFramer::comStatusIn_handler(
      FwIndexType portNum,
      Fw::Success& condition
  ) {
      // Pass-through status (standard tutorial behavior)
      this->comStatusOut_out(portNum, condition);
  }

  void RapFramer::dataReturnIn_handler(
      FwIndexType portNum,
      Fw::Buffer& data,
      const ComCfg::FrameContext& context
  ) {
      // If using a buffer manager, return ownership
      this->deallocate_out(0, data);
  }

  // ================================================================
  // Helper Functions
  // ================================================================

  void RapFramer::buildRapFrame(const Fw::Buffer& inData, Fw::Buffer& outData) {
      // TODO: Construct RAP Header (Sync, IDs, Flags, Length)
      // TODO: Copy payload from inData into outData
      // TODO: Compute & append Fletcher-16 checksum
      // TODO: Compute & append HMAC (if required for now)
  }

  U16 RapFramer::computeFletcher16(const U8* data, U32 length) {
      U16 sum1 = 0xff, sum2 = 0xff;
      while (length) {
          U32 tlen = length > 20 ? 20 : length;
          length -= tlen;
          do {
              sum1 += *data++;
              sum2 += sum1;
          } while (--tlen);
          sum1 = (sum1 & 0xff) + (sum1 >> 8);
          sum2 = (sum2 & 0xff) + (sum2 >> 8);
      }
      sum1 = (sum1 & 0xff) + (sum1 >> 8);
      sum2 = (sum2 & 0xff) + (sum2 >> 8);
      return (sum2 << 8) | sum1;
  }

  void RapFramer::computeHMAC(const U8* data, U32 length, U8* outHmac) {
      // TODO: Implement or stub SHA1-based HMAC
  }

} // namespace Drv
