#include "compress/Components/CompEngine/CompEngine.hpp"
#include "compress/Lib/CompressionLib/CompressionLib.hpp"

#include "Fw/FPrimeBasicTypes.hpp"

namespace COMP {

  // ------------------------------------------------------------------
  // Construction / init
  // ------------------------------------------------------------------

  CompEngine::CompEngine(const char* compName)
  : CompEngineComponentBase(compName)
  , m_runtimeDefaultAlgo(COMP::Algo::HUFFMAN)
  {
  }

  void CompEngine::init(FwIndexType queueDepth, FwIndexType msgSize) {
    CompEngineComponentBase::init(queueDepth, msgSize);
  }

  // ------------------------------------------------------------------
  // Helpers
  // ------------------------------------------------------------------

  bool CompEngine::algoIsValid(COMP::Algo algo) const {
    switch (algo) {
      case COMP::Algo::HUFFMAN:
      case COMP::Algo::LZSS:
      case COMP::Algo::DCT:
        return true;
      default:
        return false;
    }
  }

  U32 CompEngine::doFileCompression(
      COMP::Algo algo,
      const Fw::CmdStringArg& path,
      U32& bytesIn,
      U32& bytesOut
  ) {
    // F´ enum → u8 → lib enum
    auto libAlgo = static_cast<CompressionLib::Algorithm>(
        static_cast<std::uint8_t>(algo)
    );

    CompressionLib::Result r =
        CompressionLib::compressFile(libAlgo, path.toChar());

    bytesIn  = r.bytesIn;
    bytesOut = r.bytesOut;

    return (r.error == 0) ? 0U : static_cast<U32>(-r.error);
  }

  U32 CompEngine::doFolderCompression(
      COMP::Algo algo,
      const Fw::CmdStringArg& folder,
      U32& bytesIn,
      U32& bytesOut
  ) {
    auto libAlgo = static_cast<CompressionLib::Algorithm>(
        static_cast<std::uint8_t>(algo)
    );

    CompressionLib::Result r =
        CompressionLib::compressFolder(libAlgo, folder.toChar());

    bytesIn  = r.bytesIn;
    bytesOut = r.bytesOut;

    return (r.error == 0) ? 0U : static_cast<U32>(-r.error);
  }

  // ------------------------------------------------------------------
  // Command handlers
  // ------------------------------------------------------------------

  void CompEngine::COMPRESS_FILE_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq,
      COMP::Algo algo,
      const Fw::CmdStringArg& path
  ) {
    if (!this->algoIsValid(algo)) {
      this->log_WARNING_LO_InvalidAlgorithm(static_cast<U8>(algo));
      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::FORMAT_ERROR);
      return;
    }

    this->log_ACTIVITY_HI_CompressionRequested(algo, path);

    U32 bytesIn  = 0U;
    U32 bytesOut = 0U;
    const U32 result = this->doFileCompression(algo, path, bytesIn, bytesOut);

    if (result == 0U) {
      this->log_ACTIVITY_LO_CompressionSucceeded(bytesIn, bytesOut);

      this->tlmWrite_LastAlgo(algo);
      const F32 ratio =
          (bytesIn > 0U) ? static_cast<F32>(bytesOut) / static_cast<F32>(bytesIn) : 0.0F;
      this->tlmWrite_LastRatio(ratio);
      this->tlmWrite_LastResultCode(0U);

      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
      this->log_WARNING_HI_CompressionFailed(result);

      this->tlmWrite_LastAlgo(algo);
      this->tlmWrite_LastRatio(0.0F);
      this->tlmWrite_LastResultCode(result);

      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
  }

  void CompEngine::COMPRESS_FOLDER_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq,
      COMP::Algo algo,
      const Fw::CmdStringArg& folder
  ) {
    if (!this->algoIsValid(algo)) {
      this->log_WARNING_LO_InvalidAlgorithm(static_cast<U8>(algo));
      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::FORMAT_ERROR);
      return;
    }

    this->log_ACTIVITY_HI_CompressionRequested(algo, folder);

    U32 bytesIn  = 0U;
    U32 bytesOut = 0U;
    const U32 result = this->doFolderCompression(algo, folder, bytesIn, bytesOut);

    if (result == 0U) {
      this->log_ACTIVITY_LO_CompressionSucceeded(bytesIn, bytesOut);

      this->tlmWrite_LastAlgo(algo);
      const F32 ratio =
          (bytesIn > 0U) ? static_cast<F32>(bytesOut) / static_cast<F32>(bytesIn) : 0.0F;
      this->tlmWrite_LastRatio(ratio);
      this->tlmWrite_LastResultCode(0U);

      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
      this->log_WARNING_HI_CompressionFailed(result);

      this->tlmWrite_LastAlgo(algo);
      this->tlmWrite_LastRatio(0.0F);
      this->tlmWrite_LastResultCode(result);

      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
  }

  void CompEngine::SET_DEFAULT_ALGO_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq,
      COMP::Algo algo
  ) {
    if (!this->algoIsValid(algo)) {
      this->log_WARNING_LO_InvalidAlgorithm(static_cast<U8>(algo));
      this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::INVALID_OPCODE);
      return;
    }

    this->m_runtimeDefaultAlgo = algo;
    this->tlmWrite_LastAlgo(algo);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
  }

  void CompEngine::PING_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq,
      U32 key
  ) {
    (void)key;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
  }

} // namespace COMP
