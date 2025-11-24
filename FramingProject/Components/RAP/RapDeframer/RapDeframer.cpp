// ======================================================================
// \title  RapDeframer.cpp
// \brief  Deframer component for the RAP packn protocl placeholder. Use fprime-util impl
// ======================================================================
#include "Components/RAP/RapDeframer/RapDeframer.hpp"
#include "Fw/Logger/Logger.hpp"

namespace Drv {

  RapDeframer::RapDeframer(const char* const compName) :
      RapDeframerComponentBase(compName),
      m_state(WAIT_FOR_SYNC),
      m_expectedSize(0),
      m_bytesRead(0) {
  }

  RapDeframer::~RapDeframer() {}

  // ================================================================
  // Handlers
  // ================================================================

  void RapDeframer::bufferIn_handler(
      FwIndexType portNum,
      Fw::Buffer& buffer
  ) {
      const U8* data = buffer.getData();
      const U32 size = buffer.getSize();

      // TODO: Implement state machine for RAP extraction
      Fw::Buffer deframedBuffer;
      if (this->tryExtractRapPacket(data, size, deframedBuffer)) {
          // Emit telemetry event
          this->log_ACTIVITY_HI_PacketDeframed(deframedBuffer.getSize());

          // Pass deframed packet upstream
          this->deframedOut_out(0, deframedBuffer);
      }

      // Return raw buffer to manager
      this->bufferReturnIn_handler(0, buffer);
  }

  void RapDeframer::bufferReturnIn_handler(
      FwIndexType portNum,
      Fw::Buffer& buffer
  ) {
      // Return unused buffers to the manager
      this->deallocate_out(0, buffer);
  }

  void RapDeframer::comStatusIn_handler(
      FwIndexType portNum,
      Fw::Success& condition
  ) {
      // Pass through status unchanged
      this->comStatusOut_out(portNum, condition);
  }

  void RapDeframer::reset(void) {
      // Reset internal state machine
      m_state = WAIT_FOR_SYNC;
      m_expectedSize = 0;
      m_bytesRead = 0;
  }

  // ================================================================
  // Helper Functions
  // ================================================================

  bool RapDeframer::tryExtractRapPacket(
      const U8* data,
      const U32 size,
      Fw::Buffer& outBuffer
  ) {
      // TODO: Implement state machine:
      // 1. Search for sync (0xAB 0xCD)
      // 2. Read header, determine length
      // 3. Read full packet into m_buffer
      // 4. Validate Fletcher checksum
      // 5. Copy payload into outBuffer

      return false;  // Placeholder until implemented
  }

  bool RapDeframer::validateChecksum(const U8* data, U32 size) {
      // TODO: Validate RAP checksum
      return true;
  }

  U16 RapDeframer::computeFletcher16(const U8* data, U32 length) {
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

} // namespace Drv
