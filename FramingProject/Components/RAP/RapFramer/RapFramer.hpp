// ======================================================================
// \title  RapFramer.hpp
// \brief  Framer component for the RAP packetization protocol placeholder. Use fprime-util impl
// ======================================================================
#ifndef DRV_RAPFRAMER_HPP
#define DRV_RAPFRAMER_HPP

#include "Components/RAP/RapFramer/RapFramerComponentAc.hpp"  // Auto-generated from your FPP
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "fprime/default/config/FrameContextSerializableAc.hpp"

namespace Drv {

  class RapFramer : public RapFramerComponentBase {
    public:

      // Constructor / Destructor
      RapFramer(const char* const compName);
      ~RapFramer();

    PRIVATE:
      // ========== Required Handlers ==========

      //! Handle incoming data to be framed
      void dataIn_handler(
          FwIndexType portNum,
          Fw::Buffer& data,
          const ComCfg::FrameContext& context
      ) override;

      //! Handle com status passthrough (pass upstream unless needed)
      void comStatusIn_handler(
          FwIndexType portNum,
          Fw::Success& condition
      ) override;

      //! Handle returned buffers (e.g., return to buffer manager)
      void dataReturnIn_handler(
          FwIndexType portNum,
          Fw::Buffer& data,
          const ComCfg::FrameContext& context
      ) override;

      // ========== Helper Functions ==========

      //! Build a RAP frame (header + checksum + HMAC)
      void buildRapFrame(const Fw::Buffer& inData, Fw::Buffer& outData);

      //! Compute Fletcher-16 checksum (stub for now)
      U16 computeFletcher16(const U8* data, U32 length);

      //! (Optional) Compute HMAC (stub for now)
      void computeHMAC(const U8* data, U32 length, U8* outHmac);

  };

} // namespace Drv

#endif
