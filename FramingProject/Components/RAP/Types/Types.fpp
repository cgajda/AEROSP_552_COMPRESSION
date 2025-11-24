module  RAP{
module Types {


    @ Describes the frame header format, TODO: Implement RAP head wrapper
    struct FrameHeader {
        startWord: U32,
        lengthField: U32
    } default {
        startWord = 0xDECAFBAD
    }

    @ Describes the frame trailer format, TODO: Implement RAP tail wrapper
    struct FrameTrailer {
        crcField: U32
    }


}
}