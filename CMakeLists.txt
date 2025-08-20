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


find_package(Protobuf QUIET)

# Find ROOT (required for Monitor library)
find_package(ROOT QUIET)
if(ROOT_FOUND)
    message(STATUS "Found ROOT at: ${ROOT_INCLUDE_DIRS}")
else()
    message(WARNING "ROOT not found - Monitor library will not have full functionality")
endif()

# Find CAEN FELib
find_library(CAEN_FELIB CAEN_FELib)
if(CAEN_FELIB)
    message(STATUS "Found CAEN_FELib: ${CAEN_FELIB}")
endif()

# Add subdirectories
# Component libraries built directly into DELILA
# add_subdirectory(lib/digitizer)
# add_subdirectory(lib/net)

# Collect all source files from lib subdirectories, excluding duplicates
file(GLOB_RECURSE DIGITIZER_SOURCES "lib/digitizer/src/*.cpp")
file(GLOB_RECURSE NET_SOURCES "lib/net/src/*.cpp")
file(GLOB_RECURSE MONITOR_SOURCES "lib/monitor/src/*.cpp")

# Remove duplicate EventData.cpp - use net version (contains serialization)
list(FILTER DIGITIZER_SOURCES EXCLUDE REGEX ".*EventData\.cpp$")

set(DELILA_SOURCES ${DIGITIZER_SOURCES} ${NET_SOURCES} ${MONITOR_SOURCES})

# Create a simple main source file for version info
set(DELILA_MAIN_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/delila_main.cpp")
file(WRITE ${DELILA_MAIN_SOURCE} 
"// Main source file for DELILA library
#include <iostream>

namespace DELILA {
    const char* getVersion() {
        return \"${PROJECT_VERSION}\";
    }
    
    bool initialize(const char* config_path = nullptr) {
        std::cout << \"DELILA2 v\" << getVersion() << \" initialized\" << std::endl;
        return true;
    }
    
    void shutdown() {
        std::cout << \"DELILA2 shutdown complete\" << std::endl;
    }
}
")

# Create unified DELILA shared library with all sources
add_library(DELILA SHARED 
    ${DELILA_MAIN_SOURCE}
    ${DELILA_SOURCES}
)

# Add include directories
target_include_directories(DELILA
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/digitizer/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/net/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/monitor/include>
        $<INSTALL_INTERFACE:include>
)

# Link external dependencies
if(ZMQ_FOUND)
    target_link_libraries(DELILA PUBLIC ${ZMQ_LIBRARIES})
    target_include_directories(DELILA PUBLIC ${ZMQ_INCLUDE_DIRS})
endif()
if(LZ4_FOUND)
    target_link_libraries(DELILA PUBLIC ${LZ4_LIBRARIES})
    target_include_directories(DELILA PUBLIC ${LZ4_INCLUDE_DIRS})
endif()
if(Protobuf_FOUND)
    target_link_libraries(DELILA PUBLIC ${Protobuf_LIBRARIES})
    target_include_directories(DELILA PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()
if(CAEN_FELIB)
    target_link_libraries(DELILA PUBLIC ${CAEN_FELIB})
endif()
if(ROOT_FOUND)
    # Get ROOT library directory
    execute_process(COMMAND root-config --libdir
        OUTPUT_VARIABLE ROOT_LIB_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # Find RHTTP library specifically
    find_library(ROOT_RHTTP_LIBRARY 
        NAMES RHTTP libRHTTP
        PATHS ${ROOT_LIB_DIR}
        NO_DEFAULT_PATH
    )
    
    if(ROOT_RHTTP_LIBRARY)
        target_link_libraries(DELILA PUBLIC ${ROOT_LIBRARIES} ${ROOT_RHTTP_LIBRARY})
        message(STATUS "Found ROOT RHTTP library: ${ROOT_RHTTP_LIBRARY}")
    else()
        target_link_libraries(DELILA PUBLIC ${ROOT_LIBRARIES})
        message(WARNING "ROOT RHTTP library not found - THttpServer will not work")
    endif()
    
    target_include_directories(DELILA PUBLIC ${ROOT_INCLUDE_DIRS})
    target_compile_definitions(DELILA PUBLIC ROOT_FOUND)
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
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Option to build tests
option(BUILD_TESTS "Build tests" ON)

# Enable testing only if requested
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add examples (commented out for standalone library build)
# add_subdirectory(examples)

# Install configuration
install(TARGETS DELILA
    EXPORT DELILA2Targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/delila
)

# Install all headers with proper directory structure
install(DIRECTORY include/delila
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Install core library headers
install(DIRECTORY include/delila/core/
    DESTINATION include/delila/core
    FILES_MATCHING PATTERN "*.hpp"
)

# Install digitizer library headers
install(DIRECTORY lib/digitizer/include/
    DESTINATION include/delila/digitizer
    FILES_MATCHING PATTERN "*.hpp"
)

# Install network library headers  
install(DIRECTORY lib/net/include/
    DESTINATION include/delila/net
    FILES_MATCHING PATTERN "*.hpp"
)

# Install monitor library headers
install(DIRECTORY lib/monitor/include/
    DESTINATION include/delila/monitor
    FILES_MATCHING PATTERN "*.hpp"
)

# Export targets for other CMake projects
install(EXPORT DELILA2Targets
    FILE DELILA2Targets.cmake
    NAMESPACE DELILA2::
    DESTINATION lib/cmake/DELILA2
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