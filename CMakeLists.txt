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

# Optional dependencies for network functionality
# Direct path specification for ZMQ on macOS
if(EXISTS "/opt/homebrew/lib/libzmq.dylib")
    set(ZMQ_LIBRARIES "/opt/homebrew/lib/libzmq.dylib")
    set(ZMQ_INCLUDE_DIRS "/opt/homebrew/include")
    set(ZMQ_FOUND TRUE)
    message(STATUS "Found ZMQ at: ${ZMQ_LIBRARIES}")
elseif(EXISTS "/usr/local/lib/libzmq.dylib")
    set(ZMQ_LIBRARIES "/usr/local/lib/libzmq.dylib")
    set(ZMQ_INCLUDE_DIRS "/usr/local/include")
    set(ZMQ_FOUND TRUE)
    message(STATUS "Found ZMQ at: ${ZMQ_LIBRARIES}")
elseif(EXISTS "/usr/lib/libzmq.dylib")
    set(ZMQ_LIBRARIES "/usr/lib/libzmq.dylib")
    set(ZMQ_INCLUDE_DIRS "/usr/include")
    set(ZMQ_FOUND TRUE)
    message(STATUS "Found ZMQ at: ${ZMQ_LIBRARIES}")
else()
    # Fallback to pkg-config
    pkg_check_modules(ZMQ libzmq)
endif()

# Direct path specification for LZ4 on macOS
if(EXISTS "/opt/homebrew/lib/liblz4.dylib")
    set(LZ4_LIBRARIES "/opt/homebrew/lib/liblz4.dylib")
    set(LZ4_INCLUDE_DIRS "/opt/homebrew/include")
    set(LZ4_FOUND TRUE)
    message(STATUS "Found LZ4 at: ${LZ4_LIBRARIES}")
elseif(EXISTS "/usr/local/lib/liblz4.dylib")
    set(LZ4_LIBRARIES "/usr/local/lib/liblz4.dylib")
    set(LZ4_INCLUDE_DIRS "/usr/local/include")
    set(LZ4_FOUND TRUE)
    message(STATUS "Found LZ4 at: ${LZ4_LIBRARIES}")
elseif(EXISTS "/usr/lib/liblz4.dylib")
    set(LZ4_LIBRARIES "/usr/lib/liblz4.dylib")
    set(LZ4_INCLUDE_DIRS "/usr/include")
    set(LZ4_FOUND TRUE)
    message(STATUS "Found LZ4 at: ${LZ4_LIBRARIES}")
else()
    pkg_check_modules(LZ4 liblz4)
endif()

# Direct path specification for xxHash on macOS
if(EXISTS "/opt/homebrew/lib/libxxhash.dylib")
    set(XXHASH_LIBRARIES "/opt/homebrew/lib/libxxhash.dylib")
    set(XXHASH_INCLUDE_DIRS "/opt/homebrew/include")
    set(XXHASH_FOUND TRUE)
    message(STATUS "Found xxHash at: ${XXHASH_LIBRARIES}")
elseif(EXISTS "/usr/local/lib/libxxhash.dylib")
    set(XXHASH_LIBRARIES "/usr/local/lib/libxxhash.dylib")
    set(XXHASH_INCLUDE_DIRS "/usr/local/include")
    set(XXHASH_FOUND TRUE)
    message(STATUS "Found xxHash at: ${XXHASH_LIBRARIES}")
elseif(EXISTS "/usr/lib/libxxhash.dylib")
    set(XXHASH_LIBRARIES "/usr/lib/libxxhash.dylib")
    set(XXHASH_INCLUDE_DIRS "/usr/include")
    set(XXHASH_FOUND TRUE)
    message(STATUS "Found xxHash at: ${XXHASH_LIBRARIES}")
else()
    pkg_check_modules(XXHASH libxxhash)
endif()

find_package(Protobuf QUIET)

# Add subdirectories
add_subdirectory(lib/digitizer)
add_subdirectory(lib/net)

# Create a main library source file
set(DELILA_MAIN_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/delila_main.cpp")
file(WRITE ${DELILA_MAIN_SOURCE} 
"// Main source file for DELILA library
#include <iostream>

namespace DELILA {
    const char* getVersion() {
        return \"1.0.0\";
    }
    
    bool initialize(const char* config_path) {
        // TODO: Implement initialization
        return true;
    }
    
    void shutdown() {
        // TODO: Implement shutdown
    }
}
")

# Create unified libDELILA.so
add_library(DELILA SHARED ${DELILA_MAIN_SOURCE})
# Link libraries conditionally
target_link_libraries(DELILA PUBLIC delila_digitizer delila_net)

# Link optional dependencies if found
if(ZMQ_FOUND)
    target_link_libraries(DELILA PRIVATE ${ZMQ_LIBRARIES})
endif()
if(LZ4_FOUND)
    target_link_libraries(DELILA PRIVATE ${LZ4_LIBRARIES})
endif()
if(XXHASH_FOUND)
    target_link_libraries(DELILA PRIVATE ${XXHASH_LIBRARIES})
endif()
if(Protobuf_FOUND)
    target_link_libraries(DELILA PRIVATE ${Protobuf_LIBRARIES})
endif()

target_include_directories(DELILA
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Add optional include directories
if(ZMQ_FOUND)
    target_include_directories(DELILA PRIVATE ${ZMQ_INCLUDE_DIRS})
endif()
if(LZ4_FOUND)
    target_include_directories(DELILA PRIVATE ${LZ4_INCLUDE_DIRS})
endif()
if(XXHASH_FOUND)
    target_include_directories(DELILA PRIVATE ${XXHASH_INCLUDE_DIRS})
endif()
if(Protobuf_FOUND)
    target_include_directories(DELILA PRIVATE ${Protobuf_INCLUDE_DIRS})
endif()

# Compiler definitions
if(ZMQ_FOUND)
    target_compile_definitions(DELILA PRIVATE ${ZMQ_CFLAGS_OTHER})
endif()

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