cmake_minimum_required(VERSION 3.16)
project(websocket_asr_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# add ./include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Try to find sherpa-onnx via pkg-config first
pkg_check_modules(SHERPA_ONNX sherpa-onnx)

if(NOT SHERPA_ONNX_FOUND)
    message(STATUS "sherpa-onnx not found via pkg-config")
    
    # Try to find pre-installed libraries
    find_library(SHERPA_ONNX_CORE_LIB sherpa-onnx-core PATHS /usr/local/lib /usr/lib)
    find_library(SHERPA_ONNX_CXX_API_LIB sherpa-onnx-cxx-api PATHS /usr/local/lib /usr/lib)
    find_library(SHERPA_ONNX_C_API_LIB sherpa-onnx-c-api PATHS /usr/local/lib /usr/lib)
    find_library(SHERPA_ONNX_FST_LIB sherpa-onnx-fst PATHS /usr/local/lib /usr/lib)
    find_library(SHERPA_ONNX_FSTFAR_LIB sherpa-onnx-fstfar PATHS /usr/local/lib /usr/lib)
    find_library(SHERPA_ONNX_KALDIFST_LIB sherpa-onnx-kaldifst-core PATHS /usr/local/lib /usr/lib)
    
    # Find additional required dependencies
    find_library(ONNXRUNTIME_LIB onnxruntime PATHS /usr/local/lib /usr/lib)
    find_library(KALDI_DECODER_LIB kaldi-decoder-core PATHS /usr/local/lib /usr/lib)
    find_library(KALDI_NATIVE_FBANK_LIB kaldi-native-fbank-core PATHS /usr/local/lib /usr/lib)
    find_library(SSENTENCEPIECE_LIB ssentencepiece_core PATHS /usr/local/lib /usr/lib)
    find_library(ESPEAK_NG_LIB espeak-ng PATHS /usr/local/lib /usr/lib)
    find_library(PIPER_PHONEMIZE_LIB piper_phonemize PATHS /usr/local/lib /usr/lib)
    find_library(KISSFFT_LIB kissfft-float PATHS /usr/local/lib /usr/lib)
    find_library(UCD_LIB ucd PATHS /usr/local/lib /usr/lib)
    
    if(SHERPA_ONNX_CORE_LIB AND SHERPA_ONNX_CXX_API_LIB)
        set(SHERPA_ONNX_LIBRARIES 
            ${SHERPA_ONNX_CXX_API_LIB}
            ${SHERPA_ONNX_C_API_LIB}
            ${SHERPA_ONNX_CORE_LIB}
            ${SHERPA_ONNX_FST_LIB}
            ${SHERPA_ONNX_FSTFAR_LIB}
            ${SHERPA_ONNX_KALDIFST_LIB}
            ${ONNXRUNTIME_LIB}
            ${KALDI_DECODER_LIB}
            ${KALDI_NATIVE_FBANK_LIB}
            ${SSENTENCEPIECE_LIB}
            ${ESPEAK_NG_LIB}
            ${PIPER_PHONEMIZE_LIB}
            ${KISSFFT_LIB}
            ${UCD_LIB}
        )
        set(SHERPA_ONNX_INCLUDE_DIRS /usr/local/include)
        set(SHERPA_ONNX_FOUND TRUE)
        message(STATUS "Found sherpa-onnx libraries:")
        message(STATUS "  Core: ${SHERPA_ONNX_CORE_LIB}")
        message(STATUS "  CXX API: ${SHERPA_ONNX_CXX_API_LIB}")
        message(STATUS "  C API: ${SHERPA_ONNX_C_API_LIB}")
        message(STATUS "  OnnxRuntime: ${ONNXRUNTIME_LIB}")
        message(STATUS "  Kaldi Decoder: ${KALDI_DECODER_LIB}")
        message(STATUS "  Kaldi Native Fbank: ${KALDI_NATIVE_FBANK_LIB}")
    else()
        message(STATUS "sherpa-onnx not found. Building without sherpa-onnx support.")
        message(STATUS "Note: To enable speech recognition, please install sherpa-onnx:")
        message(STATUS "  - Install via package manager, or")
        message(STATUS "  - Build and install sherpa-onnx separately")
        # Create empty variables for compilation
        set(SHERPA_ONNX_LIBRARIES "")
        set(SHERPA_ONNX_INCLUDE_DIRS "")
        set(SHERPA_ONNX_FOUND FALSE)
    endif()
endif()

# Find websocketpp
find_path(WEBSOCKETPP_INCLUDE_DIR
    NAMES websocketpp/config/asio_no_tls.hpp
    PATHS /usr/local/include /usr/include
)

if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(FATAL_ERROR "websocketpp not found. Please install websocketpp first.")
endif()

# Find jsoncpp
pkg_check_modules(JSONCPP jsoncpp)
if(NOT JSONCPP_FOUND)
    find_path(JSONCPP_INCLUDE_DIR
        NAMES json/json.h
        PATHS /usr/local/include /usr/include
    )
    find_library(JSONCPP_LIBRARY
        NAMES jsoncpp
        PATHS /usr/local/lib /usr/lib
    )
    if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
        message(FATAL_ERROR "jsoncpp not found. Please install jsoncpp first.")
    endif()
    set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
    set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
endif()

# Find standalone ASIO or Boost ASIO
find_path(ASIO_INCLUDE_DIR
    NAMES asio/version.hpp
    PATHS /usr/local/include /usr/include
)

if(ASIO_INCLUDE_DIR)
    message(STATUS "Found standalone ASIO: ${ASIO_INCLUDE_DIR}")
    set(USE_STANDALONE_ASIO TRUE)
else()
    message(STATUS "Standalone ASIO not found, trying Boost ASIO")
    find_package(Boost REQUIRED COMPONENTS system)
    set(USE_STANDALONE_ASIO FALSE)
    message(STATUS "Using Boost ASIO")
endif()

# Include directories
include_directories(
    ${SHERPA_ONNX_INCLUDE_DIRS}
    ${WEBSOCKETPP_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

if(USE_STANDALONE_ASIO)
    include_directories(${ASIO_INCLUDE_DIR})
else()
    include_directories(${Boost_INCLUDE_DIRS})
endif()

# Create executables with multiple source files
add_executable(websocket_asr_server 
    main.cpp
    src/asr_engine.cpp
    src/asr_session.cpp
    src/websocket_server.cpp
    src/logger.cpp
)

# Link libraries for websocket_asr_server
target_link_libraries(websocket_asr_server
    ${SHERPA_ONNX_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

if(NOT USE_STANDALONE_ASIO)
    target_link_libraries(websocket_asr_server ${Boost_LIBRARIES})
endif()

# Compiler definitions for websocketpp
if(USE_STANDALONE_ASIO)
    target_compile_definitions(websocket_asr_server PRIVATE
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_STL_
    )
else()
    target_compile_definitions(websocket_asr_server PRIVATE
        _WEBSOCKETPP_CPP11_STL_
    )
endif()

# Installation
install(TARGETS websocket_asr_server
    RUNTIME DESTINATION bin
)
