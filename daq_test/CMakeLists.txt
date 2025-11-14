cmake_minimum_required(VERSION 3.16)
project(DELILA_Test 
    VERSION 1.0.0
    DESCRIPTION "DELILA digitizer test program"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug symbols for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

# Find required libraries
find_library(DELILA_LIB 
    NAMES DELILA libDELILA
    PATHS /usr/local/lib
    REQUIRED
)

find_library(CAEN_FELIB 
    NAMES CAEN_FELib libCAEN_FELib
    PATHS /usr/local/lib
    REQUIRED
)

find_library(CAEN_DIG2 
    NAMES CAEN_DIG2 libCAEN_Dig2
    PATHS /usr/local/lib
    REQUIRED
)

# Use local project headers instead of installed ones
set(DELILA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(DELILA_NET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/net/include)
set(DELILA_DIGITIZER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/digitizer/include)

# Find ROOT using newer CMake approach
list(APPEND CMAKE_PREFIX_PATH "/opt/ROOT")
find_package(ROOT REQUIRED COMPONENTS Core Hist Graf Gpad RIO Net RHTTP)

# Find network libraries
find_library(DELILA_NET_LIB 
    NAMES delila_net libdelila_net
    PATHS /usr/local/lib
    REQUIRED
)

# Monitor functionality is part of the main DELILA library

# Platform-specific network library paths
if(APPLE)
    # macOS with Homebrew
    if(EXISTS "/opt/homebrew/lib/libzmq.dylib")
        set(ZMQ_LIBRARIES /opt/homebrew/lib/libzmq.dylib)
        set(LZ4_LIBRARIES /opt/homebrew/lib/liblz4.dylib)
        set(XXHASH_LIBRARIES /opt/homebrew/lib/libxxhash.dylib)
    elseif(EXISTS "/usr/local/lib/libzmq.dylib")
        set(ZMQ_LIBRARIES /usr/local/lib/libzmq.dylib)
        set(LZ4_LIBRARIES /usr/local/lib/liblz4.dylib)
        set(XXHASH_LIBRARIES /usr/local/lib/libxxhash.dylib)
    endif()
elseif(UNIX)
    # Linux
    find_library(ZMQ_LIBRARIES NAMES zmq libzmq
        PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)
    find_library(LZ4_LIBRARIES NAMES lz4 liblz4
        PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)
    find_library(XXHASH_LIBRARIES NAMES xxhash libxxhash
        PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)
endif()

# Create executables
add_executable(delila_test main.cpp)
add_executable(delila_source source.cpp)
add_executable(delila_sink sink.cpp)
add_executable(delila_monitor monitor.cpp)
add_executable(delila_recorder recorder.cpp)
add_executable(delila_emulator emulator.cpp)

# Add ROOT-specific compile flags to handle namespace issues
target_compile_definitions(delila_test PRIVATE R__HAS_STD_SPAN)
target_compile_definitions(delila_sink PRIVATE R__HAS_STD_SPAN)
target_compile_definitions(delila_monitor PRIVATE R__HAS_STD_SPAN)
target_compile_definitions(delila_recorder PRIVATE R__HAS_STD_SPAN)
target_compile_definitions(delila_emulator PRIVATE R__HAS_STD_SPAN)

# Set target properties
target_compile_features(delila_test PRIVATE cxx_std_17)
target_compile_features(delila_source PRIVATE cxx_std_17)
target_compile_features(delila_sink PRIVATE cxx_std_17)
target_compile_features(delila_monitor PRIVATE cxx_std_17)
target_compile_features(delila_recorder PRIVATE cxx_std_17)
target_compile_features(delila_emulator PRIVATE cxx_std_17)

# Include directories (use local project headers)
target_include_directories(delila_test PRIVATE ${DELILA_INCLUDE_DIR} ${DELILA_NET_INCLUDE_DIR} ${DELILA_DIGITIZER_INCLUDE_DIR})
target_include_directories(delila_source PRIVATE ${DELILA_INCLUDE_DIR} ${DELILA_NET_INCLUDE_DIR} ${DELILA_DIGITIZER_INCLUDE_DIR})
target_include_directories(delila_sink PRIVATE ${DELILA_INCLUDE_DIR} ${DELILA_NET_INCLUDE_DIR} ${ROOT_INCLUDE_DIRS})
target_include_directories(delila_monitor PRIVATE ${DELILA_INCLUDE_DIR} ${DELILA_NET_INCLUDE_DIR} ${ROOT_INCLUDE_DIRS})
target_include_directories(delila_recorder PRIVATE ${DELILA_INCLUDE_DIR} ${DELILA_NET_INCLUDE_DIR} ${ROOT_INCLUDE_DIRS})
target_include_directories(delila_emulator PRIVATE ${DELILA_INCLUDE_DIR} ${DELILA_NET_INCLUDE_DIR})

# Link libraries using modern ROOT CMake interface
target_link_libraries(delila_test PRIVATE 
    ${DELILA_LIB} 
    ${CAEN_FELIB} 
    ${CAEN_DIG2} 
    ROOT::Core 
    ROOT::Hist 
    ROOT::Graf 
    ROOT::Gpad 
    ROOT::RIO 
    ROOT::Net 
    ROOT::RHTTP
)

# Link libraries for source (no ROOT needed)
target_link_libraries(delila_source PRIVATE
    ${DELILA_LIB}
    ${CAEN_FELIB}
    ${CAEN_DIG2}
    ${ZMQ_LIBRARIES}
    ${LZ4_LIBRARIES}
    ${XXHASH_LIBRARIES}
)

# Link libraries for sink (includes ROOT for Monitor)
target_link_libraries(delila_sink PRIVATE
  ${DELILA_NET_LIB}
  ${DELILA_LIB}
  ${ZMQ_LIBRARIES}
  ${LZ4_LIBRARIES}
  ${XXHASH_LIBRARIES}
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::RIO ROOT::Net ROOT::RHTTP
)

target_link_libraries(delila_monitor PRIVATE
  ${DELILA_NET_LIB}
  ${DELILA_LIB}
  ${ZMQ_LIBRARIES}
  ${LZ4_LIBRARIES}
  ${XXHASH_LIBRARIES}
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::RIO ROOT::Net ROOT::RHTTP
)

target_link_libraries(delila_recorder PRIVATE
  ${DELILA_NET_LIB}
  ${DELILA_LIB}
  ${ZMQ_LIBRARIES}
  ${LZ4_LIBRARIES}
  ${XXHASH_LIBRARIES}
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::RIO ROOT::Net ROOT::RHTTP
)

target_link_libraries(delila_emulator PRIVATE
  ${DELILA_NET_LIB}
  ${DELILA_LIB}
  ${ZMQ_LIBRARIES}
  ${LZ4_LIBRARIES}
  ${XXHASH_LIBRARIES}
)

# Set output directory
set_target_properties(delila_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(delila_source PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(delila_sink PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(delila_monitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(delila_recorder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(delila_emulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compiler warnings
target_compile_options(delila_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_options(delila_source PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_options(delila_sink PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_options(delila_monitor PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_options(delila_recorder PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_options(delila_emulator PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)