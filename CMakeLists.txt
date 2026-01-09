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
# Platform-specific library detection for ZMQ
if(APPLE)
    # macOS with Homebrew
    if(EXISTS "/opt/homebrew/lib/libzmq.dylib")
        set(ZMQ_LIBRARIES "/opt/homebrew/lib/libzmq.dylib")
        set(ZMQ_INCLUDE_DIRS "/opt/homebrew/include")
        set(ZMQ_FOUND TRUE)
        message(STATUS "Found ZMQ (macOS Homebrew): ${ZMQ_LIBRARIES}")
    elseif(EXISTS "/usr/local/lib/libzmq.dylib")
        set(ZMQ_LIBRARIES "/usr/local/lib/libzmq.dylib")
        set(ZMQ_INCLUDE_DIRS "/usr/local/include")
        set(ZMQ_FOUND TRUE)
        message(STATUS "Found ZMQ (macOS local): ${ZMQ_LIBRARIES}")
    endif()
elseif(UNIX)
    # Linux
    if(EXISTS "/usr/lib/x86_64-linux-gnu/libzmq.so")
        set(ZMQ_LIBRARIES "/usr/lib/x86_64-linux-gnu/libzmq.so")
        set(ZMQ_INCLUDE_DIRS "/usr/include")
        set(ZMQ_FOUND TRUE)
        message(STATUS "Found ZMQ (Linux system): ${ZMQ_LIBRARIES}")
    elseif(EXISTS "/usr/local/lib/libzmq.so")
        set(ZMQ_LIBRARIES "/usr/local/lib/libzmq.so")
        set(ZMQ_INCLUDE_DIRS "/usr/local/include")
        set(ZMQ_FOUND TRUE)
        message(STATUS "Found ZMQ (Linux local): ${ZMQ_LIBRARIES}")
    elseif(EXISTS "/usr/lib/libzmq.so")
        set(ZMQ_LIBRARIES "/usr/lib/libzmq.so")
        set(ZMQ_INCLUDE_DIRS "/usr/include")
        set(ZMQ_FOUND TRUE)
        message(STATUS "Found ZMQ (Linux standard): ${ZMQ_LIBRARIES}")
    endif()
endif()

# Fallback to pkg-config if not found via direct paths
if(NOT ZMQ_FOUND)
    pkg_check_modules(ZMQ libzmq)
    if(ZMQ_FOUND)
        # Convert -lzmq to full path to avoid linking issues
        set(ZMQ_SEARCH_PATHS ${ZMQ_LIBRARY_DIRS})
        if(APPLE)
            list(APPEND ZMQ_SEARCH_PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
        elseif(UNIX)
            list(APPEND ZMQ_SEARCH_PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)
        endif()
        
        find_library(ZMQ_FULL_PATH zmq PATHS ${ZMQ_SEARCH_PATHS})
        if(ZMQ_FULL_PATH)
            set(ZMQ_LIBRARIES "${ZMQ_FULL_PATH}")
            message(STATUS "Found ZMQ via pkg-config: ${ZMQ_LIBRARIES}")
        else()
            message(WARNING "Found ZMQ via pkg-config but couldn't resolve full path")
            set(ZMQ_FOUND FALSE)
        endif()
    else()
        message(STATUS "ZMQ not found - network functionality will be disabled")
        set(ZMQ_FOUND FALSE)
    endif()
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
# Only include digitizer sources if CAEN FELib is available
if(CAEN_FELIB)
    file(GLOB_RECURSE DIGITIZER_SOURCES "lib/digitizer/src/*.cpp")
    message(STATUS "CAEN FELib found - digitizer functionality enabled")
else()
    set(DIGITIZER_SOURCES "")
    message(STATUS "CAEN FELib not found - digitizer functionality disabled")
endif()

# Add network sources only if ZMQ is available
if(ZMQ_FOUND)
    file(GLOB_RECURSE NET_SOURCES "lib/net/src/*.cpp")
    # Remove duplicate EventData.cpp - use net version (contains serialization)
    list(FILTER DIGITIZER_SOURCES EXCLUDE REGEX ".*EventData\.cpp$")
    message(STATUS "Network functionality enabled")
else()
    set(NET_SOURCES "")
    message(STATUS "Network functionality disabled - ZMQ not found")
endif()

# Add component sources (requires ZMQ)
if(ZMQ_FOUND)
    file(GLOB_RECURSE COMPONENT_SOURCES "lib/component/src/*.cpp")
    message(STATUS "Component library enabled")
else()
    set(COMPONENT_SOURCES "")
endif()

set(DELILA_SOURCES ${DIGITIZER_SOURCES} ${NET_SOURCES} ${COMPONENT_SOURCES})

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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/digitizer/include>
        $<INSTALL_INTERFACE:include>
)

# Add network include directory only if network functionality is enabled
if(ZMQ_FOUND)
    target_include_directories(DELILA
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/net/include>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/component/include>
    )
endif()

# Link external dependencies
if(ZMQ_FOUND)
    target_link_libraries(DELILA PUBLIC ${ZMQ_LIBRARIES})
    target_include_directories(DELILA PUBLIC ${ZMQ_INCLUDE_DIRS})
endif()
if(CAEN_FELIB)
    target_link_libraries(DELILA PUBLIC ${CAEN_FELIB})
endif()

# Compiler definitions
if(ZMQ_FOUND)
    target_compile_definitions(DELILA PUBLIC ZMQ_FOUND)
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

# Option to build documentation
option(BUILD_DOCUMENTATION "Build API documentation with Doxygen" OFF)

# Add documentation subdirectory
if(BUILD_DOCUMENTATION OR CMAKE_BUILD_TYPE MATCHES "Release")
    add_subdirectory(docs)
endif()

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

# Install network library headers (only if network functionality is enabled)
if(ZMQ_FOUND)
    install(DIRECTORY lib/net/include/
        DESTINATION include/delila/net
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()


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

# Configuration summary
message(STATUS "")
message(STATUS "=========== DELILA2 Configuration Summary ===========")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Optional Dependencies:")
message(STATUS "  ZMQ (Network):     ${ZMQ_FOUND}")
if(CAEN_FELIB)
    message(STATUS "  CAEN FELib:        YES")
else()
    message(STATUS "  CAEN FELib:        NO")
endif()
message(STATUS "")
message(STATUS "Features:")
if(ZMQ_FOUND)
    message(STATUS "  Network functionality: ENABLED")
else()
    message(STATUS "  Network functionality: DISABLED (install libzmq3-dev/zeromq-devel)")
endif()
if(CAEN_FELIB)
    message(STATUS "  Digitizer:             ENABLED")
else()
    message(STATUS "  Digitizer:             DISABLED (CAEN FELib not found)")
endif()
message(STATUS "======================================================")
message(STATUS "")