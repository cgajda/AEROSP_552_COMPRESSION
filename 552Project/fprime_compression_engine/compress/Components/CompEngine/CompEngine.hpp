#ifndef COMP_COMPENGINE_HPP
#define COMP_COMPENGINE_HPP

#include "Fw/FPrimeBasicTypes.hpp"
#include "compress/Components/CompEngine/CompEngineComponentAc.hpp"

namespace COMP {

  class CompEngine final : public CompEngineComponentBase {
  public:
    explicit CompEngine(const char* compName);

    // active component init signature from your build
    void init(FwIndexType queueDepth, FwIndexType msgSize);

    ~CompEngine() override = default;

  private:
    // ------------------------------------------------------------------
    // Command handlers (must match Ac.hpp EXACTLY)
    // ------------------------------------------------------------------
    void COMPRESS_FILE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        COMP::Algo algo,
        const Fw::CmdStringArg& path
    ) override;

    void COMPRESS_FOLDER_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        COMP::Algo algo,
        const Fw::CmdStringArg& folder
    ) override;

    void SET_DEFAULT_ALGO_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        COMP::Algo algo
    ) override;

    void PING_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        U32 key
    ) override;

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------
    bool algoIsValid(COMP::Algo algo) const;

    // returns 0 on success, nonzero on error
    U32 doFileCompression(
        COMP::Algo algo,
        const Fw::CmdStringArg& path,
        U32& bytesIn,
        U32& bytesOut
    );

    U32 doFolderCompression(
        COMP::Algo algo,
        const Fw::CmdStringArg& folder,
        U32& bytesIn,
        U32& bytesOut
    );

  private:
    // runtime working copy of DefaultAlgo
    COMP::Algo m_runtimeDefaultAlgo;
  };

} // namespace COMP

#endif // COMP_COMPENGINE_HPP
