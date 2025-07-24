cmake_minimum_required(VERSION 3.15)
project(DELILA2 VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find dependencies
find_package(PkgConfig REQUIRED)

# ZeroMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# LZ4
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED liblz4)

# xxHash
find_package(PkgConfig REQUIRED)
pkg_check_modules(XXHASH REQUIRED libxxhash)

# Protocol Buffers
find_package(Protobuf REQUIRED)

# Add subdirectories
add_subdirectory(lib/digitizer)
add_subdirectory(lib/net)

# Create unified libDELILA.so
add_library(DELILA SHARED)
target_link_libraries(DELILA 
    PUBLIC delila_digitizer delila_net
    PRIVATE ${ZMQ_LIBRARIES} ${LZ4_LIBRARIES} ${XXHASH_LIBRARIES} ${Protobuf_LIBRARIES}
)

target_include_directories(DELILA
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${ZMQ_INCLUDE_DIRS} ${LZ4_INCLUDE_DIRS} 
        ${XXHASH_INCLUDE_DIRS} ${Protobuf_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(DELILA PRIVATE ${ZMQ_CFLAGS_OTHER})

# Set library properties
set_target_properties(DELILA PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/delila/delila.hpp"
)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Add examples
add_subdirectory(examples)

# Install configuration
install(TARGETS DELILA
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/delila
)

install(DIRECTORY include/delila
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DELILA2Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2Config.cmake"
    INSTALL_DESTINATION lib/cmake/DELILA2
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2ConfigVersion.cmake"
    DESTINATION lib/cmake/DELILA2
)