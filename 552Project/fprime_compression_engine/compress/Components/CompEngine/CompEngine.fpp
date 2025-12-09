module COMP {

    @ Compression algorithms supported by the component
    enum Algo : U8 {
        HUFFMAN = 0
        LZSS    = 1
        DCT     = 2
    }

    @ Component for compressing files/folders in the file system
    active component CompEngine {

        ##############################################################################
        #### Component-specific commands, telemetry, events, and parameters       ####
        ##############################################################################

        @ Compress a single file at 'path' using the specified algorithm.
        @ algo: 0=HUFFMAN, 1=LZSS, 2=DCT
        async command COMPRESS_FILE(
            algo: Algo,
            path: string size 1024
        ) opcode 0x00

        @ Compress an entire folder/directory. Exact behavior is up to the impl
        @ (e.g. zip-then-compress, or compress each file).
        async command COMPRESS_FOLDER(
            algo: Algo,
            folder: string size 1024
        ) opcode 0x01

        @ Decompress a file on the target system.
        async command DECOMPRESS_FILE(
            algo: Algo,
            inputPath: string size 1024
        ) opcode 0x02

        @ Set the algorithm that will be used when a request doesn’t specify one.
        async command SET_DEFAULT_ALGO(
            algo: Algo
        ) opcode 0x03

        @ Standard ping to verify command/response path.
        sync command PING(
            key: U32
        ) opcode 0x04

        ##############################################################################
        # Telemetry                                                                 #
        ##############################################################################

        @ Last algorithm actually used (0=HUFFMAN, 1=LZSS, 2=DCT)
        telemetry LastAlgo: Algo id 0 update on change format "Last compression algorithm used {}"

        @ Last compression ratio (out_bytes / in_bytes). 1.0 = no change, <1.0 = good.
        telemetry LastRatio: F32 id 1 update always format "Last compression ratio {}"

        @ Last result code from the compressor (0 = OK, nonzero = error)
        telemetry LastResultCode: U32 id 2 format "Last compression result/error code {}"

        ##############################################################################
        # Events                                                                    #
        ##############################################################################

        @ A compression request was accepted by the component
        event CompressionRequested(
            algo: Algo,
            target: string size 1024
        ) severity activity high \
          format "Compression requested: algo={}, target={}"

        @ Compression finished successfully
        event CompressionSucceeded(
            bytesIn: U32,
            bytesOut: U32
        ) severity activity low \
          format "Compression succeeded: in={}, out={}"

        @ Compression failed with an error code
        event CompressionFailed(
            code: U32
        ) severity warning high \
          format "Compression failed: code={}"

            @ A decompression command was received.
        event DecompressionRequested(
            algo: Algo,
            target: string size 1024
        ) severity activity high \
          format "Decompression requested: algo={}, target={}"

        @ A file was successfully decompressed.
        event DecompressionSucceeded(
            bytesIn: U32,
            bytesOut: U32
        ) severity activity low \
          format "Decompression succeeded: in={}, out={}"

        @ Decompression failed with a non-zero result code.
        event DecompressionFailed(
            code: U32
        ) severity warning high \
          format "Decompression failed: code={}"


        @ Command referenced an unsupported algorithm
        event InvalidAlgorithm(
            algo: U8
        ) severity warning low \
          format "Invalid compression algorithm: {}"


        ##############################################################################
        # Parameters                                                                #
        ##############################################################################

        @ Default algorithm to use when none is specified (0=HUFFMAN,1=LZSS,2=DCT)
        param DefaultAlgo: Algo

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
