cmake_minimum_required(VERSION 3.16)
project(websocket_asr_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect GCC version for conditional compilation flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
        OUTPUT_VARIABLE GCC_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Detected GCC version: ${GCC_VERSION}")
endif()

# Security and compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Add GCC version-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(GCC_VERSION VERSION_GREATER_EQUAL "14.0")
        # GCC 14+ specific flags for executable stack warnings
        message(STATUS "Using GCC 14+ specific flags")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,noexecstack")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack")
    else()
        # For older GCC versions, use alternative approach
        message(STATUS "Using flags compatible with GCC < 14")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,--noexecstack")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wa,--noexecstack")
    endif()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Add local include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find sherpa-onnx (required - must be pre-installed)
# Set up search paths - include user local directory
set(SHERPA_ONNX_SEARCH_PATHS 
    $ENV{HOME}/.local/lib 
    /usr/local/lib 
    /usr/lib
)
set(SHERPA_ONNX_INCLUDE_PATHS 
    $ENV{HOME}/.local/include 
    /usr/local/include 
    /usr/include
)

message(STATUS "Searching for sherpa-onnx in paths:")
message(STATUS "  Library paths: ${SHERPA_ONNX_SEARCH_PATHS}")
message(STATUS "  Include paths: ${SHERPA_ONNX_INCLUDE_PATHS}")

# First try to find via CMake config
find_package(sherpa-onnx QUIET CONFIG PATHS $ENV{HOME}/.local /usr/local)

if(sherpa-onnx_FOUND)
    message(STATUS "Found sherpa-onnx via CMake config: ${sherpa-onnx_DIR}")
    # Create aliases for consistency
    set(SHERPA_ONNX_LIBRARIES sherpa-onnx::sherpa-onnx)
    set(SHERPA_ONNX_FOUND TRUE)
else()
    # Try to find sherpa-onnx via pkg-config first
    pkg_check_modules(SHERPA_ONNX sherpa-onnx)

    if(NOT SHERPA_ONNX_FOUND)
        message(STATUS "sherpa-onnx not found via pkg-config")
        
        # Try to find pre-installed libraries in user and system directories
        find_library(SHERPA_ONNX_CORE_LIB sherpa-onnx-core PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(SHERPA_ONNX_CXX_API_LIB sherpa-onnx-cxx-api PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(SHERPA_ONNX_C_API_LIB sherpa-onnx-c-api PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(SHERPA_ONNX_FST_LIB sherpa-onnx-fst PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(SHERPA_ONNX_FSTFAR_LIB sherpa-onnx-fstfar PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(SHERPA_ONNX_KALDIFST_LIB sherpa-onnx-kaldifst-core PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        
        # Find additional required dependencies
        find_library(ONNXRUNTIME_LIB onnxruntime PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(KALDI_DECODER_LIB kaldi-decoder-core PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(KALDI_NATIVE_FBANK_LIB kaldi-native-fbank-core PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(SSENTENCEPIECE_LIB ssentencepiece_core PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(ESPEAK_NG_LIB espeak-ng PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(PIPER_PHONEMIZE_LIB piper_phonemize PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(KISSFFT_LIB kissfft-float PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        find_library(UCD_LIB ucd PATHS ${SHERPA_ONNX_SEARCH_PATHS})
        
        # Find include directory
        find_path(SHERPA_ONNX_INCLUDE_DIR
            NAMES sherpa-onnx/c-api/c-api.h
            PATHS ${SHERPA_ONNX_INCLUDE_PATHS}
            DOC "sherpa-onnx include directory"
        )
        
        if(SHERPA_ONNX_CORE_LIB AND SHERPA_ONNX_CXX_API_LIB AND SHERPA_ONNX_INCLUDE_DIR)
            # Filter out empty/not found libraries
            set(SHERPA_ONNX_LIBRARIES "")
            foreach(lib ${SHERPA_ONNX_CXX_API_LIB} ${SHERPA_ONNX_C_API_LIB} ${SHERPA_ONNX_CORE_LIB} 
                         ${SHERPA_ONNX_FST_LIB} ${SHERPA_ONNX_FSTFAR_LIB} ${SHERPA_ONNX_KALDIFST_LIB}
                         ${ONNXRUNTIME_LIB} ${KALDI_DECODER_LIB} ${KALDI_NATIVE_FBANK_LIB}
                         ${SSENTENCEPIECE_LIB} ${ESPEAK_NG_LIB} ${PIPER_PHONEMIZE_LIB}
                         ${KISSFFT_LIB} ${UCD_LIB})
                if(lib AND NOT lib STREQUAL "lib-NOTFOUND")
                    list(APPEND SHERPA_ONNX_LIBRARIES ${lib})
                endif()
            endforeach()
            
            set(SHERPA_ONNX_INCLUDE_DIRS ${SHERPA_ONNX_INCLUDE_DIR})
            set(SHERPA_ONNX_FOUND TRUE)
            message(STATUS "Found sherpa-onnx libraries:")
            message(STATUS "  Core: ${SHERPA_ONNX_CORE_LIB}")
            message(STATUS "  CXX API: ${SHERPA_ONNX_CXX_API_LIB}")
            message(STATUS "  C API: ${SHERPA_ONNX_C_API_LIB}")
            message(STATUS "  Include: ${SHERPA_ONNX_INCLUDE_DIR}")
            message(STATUS "  All libs: ${SHERPA_ONNX_LIBRARIES}")
        else()
            message(FATAL_ERROR "sherpa-onnx is required but not found. Please install sherpa-onnx first following the official guide:
    git clone https://github.com/k2-fsa/sherpa-onnx
    cd sherpa-onnx
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j6
    sudo make install
    
Missing components:
  Core lib: ${SHERPA_ONNX_CORE_LIB}
  CXX API: ${SHERPA_ONNX_CXX_API_LIB}  
  Include: ${SHERPA_ONNX_INCLUDE_DIR}")
        endif()
    else()
        message(STATUS "Found sherpa-onnx via pkg-config")
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
    ${WEBSOCKETPP_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

# Add sherpa-onnx include directories if available
if(SHERPA_ONNX_INCLUDE_DIRS)
    include_directories(${SHERPA_ONNX_INCLUDE_DIRS})
endif()

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
    src/oneshot_asr_session.cpp
    src/websocket_server.cpp
    src/logger.cpp
    src/model_pool.cpp
    src/server_config.cpp
)

# Link libraries for websocket_asr_server
target_link_libraries(websocket_asr_server
    ${SHERPA_ONNX_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

# Add security-related linker flags based on GCC version
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(GCC_VERSION VERSION_GREATER_EQUAL "14.0")
        # GCC 14+ specific linker flags
        target_link_options(websocket_asr_server PRIVATE
            -Wl,-z,noexecstack
            -Wl,--warn-execstack
        )
    else()
        # For older GCC versions, use basic noexecstack flag
        target_link_options(websocket_asr_server PRIVATE
            -Wl,-z,noexecstack
        )
    endif()
else()
    # For non-GCC compilers (Clang, etc.)
    target_link_options(websocket_asr_server PRIVATE
        -Wl,-z,noexecstack
    )
endif()

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
