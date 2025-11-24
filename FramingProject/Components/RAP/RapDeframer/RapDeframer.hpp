// ======================================================================
// \title  RapDeframer.hpp
// \brief  Deframer component for the RAP packn protocl placeholder. Use fprime-util impl
// ======================================================================
#ifndef DRV_RAPDEFRAMER_HPP
#define DRV_RAPDEFRAMER_HPP

#include "Components/RAP/RapDeframer/RapDeframerComponentAc.hpp"  // Auto-generated from your FPP
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "fprime/default/config/FrameContextSerializableAc.hpp"

namespace Drv {

  class RapDeframer : public RapDeframerComponentBase {
    public:

      // Constructor / Destructor
      RapDeframer(const char* const compName);
      ~RapDeframer();

    PRIVATE:
      // ========== Required Handlers ==========

      //! Handle raw incoming data stream (unframed bytes)
      void bufferIn_handler(
          FwIndexType portNum,
          Fw::Buffer& buffer
      ) override;

      //! Handle return of processed buffers (return to manager, if needed)
      void bufferReturnIn_handler(
          FwIndexType portNum,
          Fw::Buffer& buffer
      ) override;

      //! Handle com status pass-through (optional)
      void comStatusIn_handler(
          FwIndexType portNum,
          Fw::Success& condition
      ) override;

      //! Reset deframer state machine (e.g., on link reset)
      void reset(void) override;

      // ========== Helper Functions ==========

      //! Attempt to extract a full RAP packet from the stream
      bool tryExtractRapPacket(
          const U8* data,
          const U32 size,
          Fw::Buffer& outBuffer
      );

      //! Validate RAP checksums
      bool validateChecksum(const U8* data, U32 size);

      //! Fletcher-16 checksum helper
      U16 computeFletcher16(const U8* data, U32 length);

      // ========== Internal State ==========

      //! Simple state machine variables
      enum DeframeState {
          WAIT_FOR_SYNC,
          READ_HEADER,
          READ_PAYLOAD
      } m_state;

      U32 m_expectedSize;
      U32 m_bytesRead;
      U8  m_buffer[512];  // Temporary staging buffer (size TBD per RAP max)
  };

} // namespace Drv

#endif
