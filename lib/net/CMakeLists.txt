# CMakeLists.txt for DELILA2 Binary Serialization Module
cmake_minimum_required(VERSION 3.15)
project(delila_net_binary VERSION 1.0.0 LANGUAGES CXX)

# C++17 requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)  # Default to debug for development
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    add_compile_options(-Wall -Wextra -Werror -pedantic)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Build options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

# Platform detection
if(APPLE)
    add_definitions(-DDELILA_PLATFORM_MACOS)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DDELILA_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Byte order check (compile-time assertion in code)
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(FATAL_ERROR "DELILA networking library requires little-endian platform")
endif()

# Dependency management with FetchContent fallback
include(FetchContent)

# LZ4 dependency
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 QUIET liblz4>=1.9.0)
if(NOT LZ4_FOUND)
    message(STATUS "System LZ4 not found, using FetchContent")
    FetchContent_Declare(
        lz4
        GIT_REPOSITORY https://github.com/lz4/lz4.git
        GIT_TAG v1.9.4
        SOURCE_SUBDIR build/cmake
    )
    FetchContent_MakeAvailable(lz4)
    set(LZ4_LIBRARIES lz4_static)
    set(LZ4_INCLUDE_DIRS ${lz4_SOURCE_DIR}/lib)
else()
    message(STATUS "Using system LZ4: ${LZ4_VERSION}")
    # For system LZ4, use pkg-config results properly
    set(LZ4_LIBRARIES ${LZ4_LINK_LIBRARIES})
    set(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIRS})
endif()

# xxHash dependency
find_path(XXHASH_INCLUDE_DIR xxhash.h)
find_library(XXHASH_LIBRARY xxhash)
if(NOT XXHASH_INCLUDE_DIR OR NOT XXHASH_LIBRARY)
    message(STATUS "System xxHash not found, using FetchContent")
    FetchContent_Declare(
        xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v0.8.1
        SOURCE_SUBDIR cmake_unofficial
    )
    FetchContent_MakeAvailable(xxhash)
    set(XXHASH_LIBRARIES xxhash)
    set(XXHASH_INCLUDE_DIRS ${xxhash_SOURCE_DIR})
else()
    message(STATUS "Using system xxHash")
    set(XXHASH_LIBRARIES ${XXHASH_LIBRARY})
    set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIR})
endif()

# Library source files
set(DELILA_NET_BINARY_SOURCES
    src/utils/Error.cpp
    src/utils/Platform.cpp
    src/serialization/BinarySerializer.cpp
    src/test/TestDataGenerator.cpp
)

# Create static library for now (easier for testing)
add_library(delila_net_binary STATIC ${DELILA_NET_BINARY_SOURCES})

# Set target properties
set_target_properties(delila_net_binary PROPERTIES
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME delila_net_binary
)

# Include directories
target_include_directories(delila_net_binary PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${LZ4_INCLUDE_DIRS}
    ${XXHASH_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(delila_net_binary PUBLIC
    ${LZ4_LIBRARIES}
    ${XXHASH_LIBRARIES}
)

# Link directories for system libraries
if(LZ4_LIBRARY_DIRS)
    target_link_directories(delila_net_binary PUBLIC ${LZ4_LIBRARY_DIRS})
endif()

# Compiler definitions
if(LZ4_CFLAGS_OTHER)
    target_compile_definitions(delila_net_binary PRIVATE ${LZ4_CFLAGS_OTHER})
endif()

# Testing
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation (minimal for now)
include(GNUInstallDirs)

install(TARGETS delila_net_binary
    EXPORT delila_net_binary_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install headers
install(DIRECTORY include/delila
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Print configuration summary
message(STATUS "=== DELILA Binary Serialization Library Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "LZ4 libraries: ${LZ4_LIBRARIES}")
message(STATUS "xxHash libraries: ${XXHASH_LIBRARIES}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")