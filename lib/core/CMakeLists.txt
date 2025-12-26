cmake_minimum_required(VERSION 3.15)
project(DELILA_Core VERSION 1.0.0 LANGUAGES CXX)

# Header-only library
add_library(delila_core INTERFACE)

target_include_directories(delila_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# C++17 required
target_compile_features(delila_core INTERFACE cxx_std_17)

# Installation
install(DIRECTORY include/delila/core/
    DESTINATION include/delila/core
    FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS delila_core
    EXPORT DELILA_CoreTargets
)

install(EXPORT DELILA_CoreTargets
    FILE DELILA_CoreTargets.cmake
    NAMESPACE DELILA::
    DESTINATION lib/cmake/DELILA_Core
)
