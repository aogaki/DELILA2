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

# Find include directories
find_path(DELILA_INCLUDE_DIR
    NAMES delila/delila.hpp
    PATHS /usr/local/include
    REQUIRED
)

# Find ROOT using newer CMake approach
list(APPEND CMAKE_PREFIX_PATH "/opt/ROOT")
find_package(ROOT REQUIRED COMPONENTS Core Hist Graf Gpad RIO Net RHTTP)

# Create executable
add_executable(delila_test main.cpp)

# Add ROOT-specific compile flags to handle namespace issues
target_compile_definitions(delila_test PRIVATE R__HAS_STD_SPAN)

# Set target properties
target_compile_features(delila_test PRIVATE cxx_std_17)

# Include directories
target_include_directories(delila_test PRIVATE ${DELILA_INCLUDE_DIR})

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

# Set output directory
set_target_properties(delila_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compiler warnings
target_compile_options(delila_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)