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
    # Fallback to pkg-config, but convert library names to full paths
    pkg_check_modules(ZMQ libzmq)
    if(ZMQ_FOUND)
        # Convert -lzmq to full path to avoid linking issues
        find_library(ZMQ_FULL_PATH zmq PATHS ${ZMQ_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib)
        if(ZMQ_FULL_PATH)
            set(ZMQ_LIBRARIES "${ZMQ_FULL_PATH}")
            message(STATUS "Found ZMQ via pkg-config, using full path: ${ZMQ_LIBRARIES}")
        else()
            message(WARNING "Found ZMQ via pkg-config but couldn't resolve full path")
        endif()
    endif()
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
    # Fallback to pkg-config, but convert library names to full paths
    pkg_check_modules(LZ4 liblz4)
    if(LZ4_FOUND)
        find_library(LZ4_FULL_PATH lz4 PATHS ${LZ4_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib)
        if(LZ4_FULL_PATH)
            set(LZ4_LIBRARIES "${LZ4_FULL_PATH}")
            message(STATUS "Found LZ4 via pkg-config, using full path: ${LZ4_LIBRARIES}")
        else()
            message(WARNING "Found LZ4 via pkg-config but couldn't resolve full path")
        endif()
    endif()
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
    # Fallback to pkg-config, but convert library names to full paths
    pkg_check_modules(XXHASH libxxhash)
    if(XXHASH_FOUND)
        find_library(XXHASH_FULL_PATH xxhash PATHS ${XXHASH_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib)
        if(XXHASH_FULL_PATH)
            set(XXHASH_LIBRARIES "${XXHASH_FULL_PATH}")
            message(STATUS "Found xxHash via pkg-config, using full path: ${XXHASH_LIBRARIES}")
        else()
            message(WARNING "Found xxHash via pkg-config but couldn't resolve full path")
        endif()
    endif()
endif()

find_package(Protobuf QUIET)

# Find CAEN FELib
find_library(CAEN_FELIB CAEN_FELib)
if(CAEN_FELIB)
    message(STATUS "Found CAEN_FELib: ${CAEN_FELIB}")
endif()

# Add subdirectories
add_subdirectory(lib/digitizer)
add_subdirectory(lib/net)

# Create a main library source file
set(DELILA_MAIN_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/delila_main.cpp")
file(WRITE ${DELILA_MAIN_SOURCE} 
"// Main source file for DELILA library
#include \"delila/delila.hpp\"
#include <iostream>

namespace DELILA {
    const char* getVersion() {
        return \"${PROJECT_VERSION}\";
    }
    
    bool initialize(const char* config_path) {
        // Initialize network component
        // Initialize digitizer component if needed
        std::cout << \"DELILA2 v\" << getVersion() << \" initialized\" << std::endl;
        return true;
    }
    
    void shutdown() {
        // Cleanup network resources
        // Cleanup digitizer resources
        std::cout << \"DELILA2 shutdown complete\" << std::endl;
    }
}
")

# Create unified DELILA as shared library that combines all components
add_library(DELILA SHARED ${DELILA_MAIN_SOURCE})
# Link component libraries with PRIVATE to avoid symbol conflicts
target_link_libraries(DELILA PRIVATE delila_digitizer delila_net)

# Link optional dependencies if found
if(ZMQ_FOUND)
    target_link_libraries(DELILA PUBLIC ${ZMQ_LIBRARIES})
endif()
if(LZ4_FOUND)
    target_link_libraries(DELILA PUBLIC ${LZ4_LIBRARIES})
endif()
if(XXHASH_FOUND)
    target_link_libraries(DELILA PUBLIC ${XXHASH_LIBRARIES})
endif()
if(Protobuf_FOUND)
    target_link_libraries(DELILA PUBLIC ${Protobuf_LIBRARIES})
endif()
if(CAEN_FELIB)
    target_link_libraries(DELILA PUBLIC ${CAEN_FELIB})
endif()

target_include_directories(DELILA
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Add optional include directories
if(ZMQ_FOUND)
    target_include_directories(DELILA PUBLIC ${ZMQ_INCLUDE_DIRS})
endif()
if(LZ4_FOUND)
    target_include_directories(DELILA PUBLIC ${LZ4_INCLUDE_DIRS})
endif()
if(XXHASH_FOUND)
    target_include_directories(DELILA PUBLIC ${XXHASH_INCLUDE_DIRS})
endif()
if(Protobuf_FOUND)
    target_include_directories(DELILA PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()

# Compiler definitions
if(ZMQ_FOUND)
    target_compile_definitions(DELILA PUBLIC ${ZMQ_CFLAGS_OTHER})
endif()

# Set library properties for shared library
set_target_properties(DELILA PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
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