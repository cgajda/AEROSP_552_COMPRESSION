#include "compress/Components/CompEngine/CompEngine.hpp"
#include "compress/Lib/CompressionLib/CompressionLib.hpp"
#include <cstring>
#include <unistd.h> 
#include <fstream>
#include <sys/resource.h>
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

    struct CpuSample {
        long usec;  // user + system in microseconds
    };

    void sampleCpu(CpuSample& s) {
        struct rusage ru{};
        getrusage(RUSAGE_SELF, &ru);
        long userUsec = ru.ru_utime.tv_sec * 1000000L + ru.ru_utime.tv_usec;
        long sysUsec  = ru.ru_stime.tv_sec * 1000000L + ru.ru_stime.tv_usec;
        s.usec = userUsec + sysUsec;
    }

    bool readRssKiB(U32& rssKiB) {
        std::ifstream statm("/proc/self/statm");
        if (!statm) {
            return false;
        }
        long totalPages = 0;
        long residentPages = 0;
        statm >> totalPages >> residentPages;
        if (!statm) {
            return false;
        }
        long pageSize = sysconf(_SC_PAGESIZE); // bytes
        long rssBytes = residentPages * pageSize;
        rssKiB = static_cast<U32>(rssBytes / 1024);
        return true;
    }

  const char* basenameC(const char* path) {
        if (path == nullptr) return "";
        const char* slash = std::strrchr(path, '/');
        return (slash != nullptr) ? (slash + 1) : path;
    }

  U32 diffUsec(const Fw::Time& start, const Fw::Time& end) {
      const U32 sec  = end.getSeconds()  - start.getSeconds();
      const I32 usec = static_cast<I32>(end.getUSeconds()) -
                      static_cast<I32>(start.getUSeconds());
      return sec * 1000000U + static_cast<U32>(usec);
  }

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

  U32 CompEngine::doFileDecompression(
      COMP::Algo algo,
      const Fw::CmdStringArg& path,
      U32& bytesIn,
      U32& bytesOut
  ) {
      // F´ enum → CompressionLib::Algorithm
      auto libAlgo = static_cast<CompressionLib::Algorithm>(
        static_cast<std::uint8_t>(algo)
      );
      // If COMP::Algo isn't directly castable, replace with a small switch.

      const std::string inputPath(path.toChar());

      // Call into your library: it decides how to name the decompressed output file
      CompressionLib::Result res = CompressionLib::decompressFile(libAlgo, inputPath);

      // Propagate byte counts back to the command handler
      bytesIn  = res.bytesIn;   // adjust field names if different
      bytesOut = res.bytesOut;

      // Return an F´-style status code (0 == success, nonzero == error)
      return static_cast<U32>(res.error);
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

    CpuSample cpuStart{};
    sampleCpu(cpuStart);
    // --- timing start ---
    const Fw::Time start = this->getTime();

    U32 bytesIn  = 0U;
    U32 bytesOut = 0U;
    const U32 result = this->doFileCompression(algo, path, bytesIn, bytesOut);

    // --- timing end ---
    const Fw::Time end = this->getTime();
    const U32 durationUsec = diffUsec(start, end);
    CpuSample cpuEnd{};
    sampleCpu(cpuEnd);

    long cpuDeltaUsec = cpuEnd.usec - cpuStart.usec;
    float cpuPct = 0.0f;
    if (durationUsec > 0U && cpuDeltaUsec > 0L) {
        cpuPct = 100.0f * static_cast<float>(cpuDeltaUsec) /
                        static_cast<float>(durationUsec);
    }
    if (cpuPct < 0.0f)   cpuPct = 0.0f;
    if (cpuPct > 100.0f) cpuPct = 100.0f;

    U16 avgCpuTimes100 = static_cast<U16>(cpuPct * 100.0f + 0.5f); // percent × 100
    
    U32 rssKiB = 0;
    if (!readRssKiB(rssKiB)) {
        rssKiB = 0;
    }
    const U32 avgRssKiB = rssKiB;

    if (result == 0U) {
        this->log_ACTIVITY_LO_CompressionSucceeded(bytesIn, bytesOut);

        this->tlmWrite_LastAlgo(algo);
        const F32 ratio =
            (bytesIn > 0U) ? static_cast<F32>(bytesOut) / static_cast<F32>(bytesIn) : 0.0F;
        this->tlmWrite_LastRatio(ratio);
        this->tlmWrite_LastResultCode(0U);

        const char* inC  = path.toChar();                  // or inputPath.toChar()
        const char* outC = path.toChar();                  // or real output if you track it

        Fw::LogStringArg inLog (basenameC(inC));
        Fw::LogStringArg outLog(basenameC(outC));

        this->log_ACTIVITY_HI_AlgoRunSummary(
            algo,
            COMP::OperationKind::COMPRESS,  // or DECOMPRESS
            inLog,
            bytesIn,
            bytesOut,
            ratio,
            durationUsec,
            avgCpuTimes100,
            avgRssKiB
        );

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

        // CPU + time start
        CpuSample cpuStart{};
        sampleCpu(cpuStart);
        const Fw::Time start = this->getTime();

        U32 bytesIn  = 0U;
        U32 bytesOut = 0U;
        const U32 result = this->doFolderCompression(algo, folder, bytesIn, bytesOut);

        // CPU + time end
        const Fw::Time end = this->getTime();
        const U32 durationUsec = diffUsec(start, end);

        CpuSample cpuEnd{};
        sampleCpu(cpuEnd);

        long cpuDeltaUsec = cpuEnd.usec - cpuStart.usec;
        float cpuPct = 0.0f;
        if (durationUsec > 0U && cpuDeltaUsec > 0L) {
            cpuPct = 100.0f * static_cast<float>(cpuDeltaUsec) /
                            static_cast<float>(durationUsec);
        }
        if (cpuPct < 0.0f)   cpuPct = 0.0f;
        if (cpuPct > 100.0f) cpuPct = 100.0f;

        const U16 avgCpuTimes100 = static_cast<U16>(cpuPct * 100.0f + 0.5f);

        U32 rssKiB = 0;
        if (!readRssKiB(rssKiB)) {
            rssKiB = 0;
        }
        const U32 avgRssKiB = rssKiB;

        if (result == 0U) {
            this->log_ACTIVITY_LO_CompressionSucceeded(bytesIn, bytesOut);

            this->tlmWrite_LastAlgo(algo);
            const F32 ratio =
                (bytesIn > 0U) ? static_cast<F32>(bytesOut) / static_cast<F32>(bytesIn) : 0.0F;
            this->tlmWrite_LastRatio(ratio);
            this->tlmWrite_LastResultCode(0U);

            const char* inC  = folder.toChar();
            const char* outC = folder.toChar(); // or some aggregate output dir if you define one

            Fw::LogStringArg inLog (basenameC(inC));
            Fw::LogStringArg outLog(basenameC(outC));

            this->log_ACTIVITY_HI_AlgoRunSummary(
                algo,
                COMP::OperationKind::COMPRESS,
                inLog,
                bytesIn,
                bytesOut,
                ratio,
                durationUsec,
                avgCpuTimes100,
                avgRssKiB
            );

            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        } else {
            this->log_WARNING_HI_CompressionFailed(result);

            this->tlmWrite_LastAlgo(algo);
            this->tlmWrite_LastRatio(0.0F);
            this->tlmWrite_LastResultCode(result);

            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        }
    }


    void CompEngine::DECOMPRESS_FILE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        COMP::Algo algo,
        const Fw::CmdStringArg& inputPath
    ) {
        // 1) Validate algorithm
        if (!this->algoIsValid(algo)) {
            this->log_WARNING_LO_InvalidAlgorithm(static_cast<U8>(algo));
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::FORMAT_ERROR);
            return;
        }

        // (Optional) validate paths are non-empty
        if (inputPath.toChar()[0] == '\0') {
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::FORMAT_ERROR);
            return;
        }

        this->log_ACTIVITY_HI_DecompressionRequested(algo, inputPath);

        // CPU + time start
        CpuSample cpuStart{};
        sampleCpu(cpuStart);
        const Fw::Time start = this->getTime();

        // 3) Run decompression
        U32 bytesIn  = 0U;
        U32 bytesOut = 0U;
        const U32 result = this->doFileDecompression(
            algo,
            inputPath,
            bytesIn,
            bytesOut
        );

        // CPU + time end
        const Fw::Time end = this->getTime();
        const U32 durationUsec = diffUsec(start, end);

        CpuSample cpuEnd{};
        sampleCpu(cpuEnd);

        long cpuDeltaUsec = cpuEnd.usec - cpuStart.usec;
        float cpuPct = 0.0f;
        if (durationUsec > 0U && cpuDeltaUsec > 0L) {
            cpuPct = 100.0f * static_cast<float>(cpuDeltaUsec) /
                            static_cast<float>(durationUsec);
        }
        if (cpuPct < 0.0f)   cpuPct = 0.0f;
        if (cpuPct > 100.0f) cpuPct = 100.0f;

        const U16 avgCpuTimes100 = static_cast<U16>(cpuPct * 100.0f + 0.5f);

        U32 rssKiB = 0;
        if (!readRssKiB(rssKiB)) {
            rssKiB = 0;
        }
        const U32 avgRssKiB = rssKiB;

        // 4) Handle result: mirror COMPRESS_FILE behavior
        if (result == 0U) {
            this->log_ACTIVITY_LO_DecompressionSucceeded(bytesIn, bytesOut);

            this->tlmWrite_LastAlgo(algo);
            const F32 ratio =
                (bytesIn > 0U) ? static_cast<F32>(bytesOut) / static_cast<F32>(bytesIn) : 0.0F;
            this->tlmWrite_LastRatio(ratio);
            this->tlmWrite_LastResultCode(0U);

            const char* inC  = inputPath.toChar();
            const char* outC = inputPath.toChar(); // replace with real output path if Result exposes it

            Fw::LogStringArg inLog (basenameC(inC));
            Fw::LogStringArg outLog(basenameC(outC));

            this->log_ACTIVITY_HI_AlgoRunSummary(
                algo,
                COMP::OperationKind::DECOMPRESS,
                inLog,
                bytesIn,
                bytesOut,
                ratio,
                durationUsec,
                avgCpuTimes100,
                avgRssKiB
            );

            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        } else {
            this->log_WARNING_HI_DecompressionFailed(result);

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
