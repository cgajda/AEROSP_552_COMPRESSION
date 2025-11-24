# Include project-wide components here

# add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MyComponent")
register_fprime_library(

    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/CompressionLib.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Huffman.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Lzss.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Dct.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/CompressionLib.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Huffman.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Lzss.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Dct.hpp"
)
