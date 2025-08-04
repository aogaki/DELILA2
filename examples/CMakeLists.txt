cmake_minimum_required(VERSION 3.15)
project(DELILA2Examples LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# When building as part of main project, DELILA target is available
# When building standalone, find installed DELILA2
if(TARGET DELILA)
    # Building as part of main project - use local target
    message(STATUS "Using local DELILA target")
    set(DELILA_TARGET DELILA)
else()
    # Building standalone - find installed DELILA2
    find_package(DELILA2 REQUIRED)
    set(DELILA_TARGET DELILA2::DELILA)
endif()

# Find dependencies that DELILA2 uses
find_package(PkgConfig REQUIRED)

# Find ZMQ
pkg_check_modules(ZMQ libzmq)
if(ZMQ_FOUND AND NOT ZMQ_LIBRARIES MATCHES "^/")
    # pkg-config found it but only gave library name, find full path
    find_library(ZMQ_FULL_PATH
        NAMES ${ZMQ_LIBRARIES}
        PATHS ${ZMQ_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(ZMQ_FULL_PATH)
        set(ZMQ_LIBRARIES ${ZMQ_FULL_PATH})
    endif()
endif()
if(NOT ZMQ_FOUND)
    # Try to find ZMQ manually
    find_path(ZMQ_INCLUDE_DIRS zmq.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    find_library(ZMQ_LIBRARIES
        NAMES zmq
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(ZMQ_INCLUDE_DIRS AND ZMQ_LIBRARIES)
        set(ZMQ_FOUND TRUE)
    endif()
endif()

# Find LZ4
pkg_check_modules(LZ4 liblz4)
if(LZ4_FOUND AND NOT LZ4_LIBRARIES MATCHES "^/")
    # pkg-config found it but only gave library name, find full path
    find_library(LZ4_FULL_PATH
        NAMES ${LZ4_LIBRARIES}
        PATHS ${LZ4_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(LZ4_FULL_PATH)
        set(LZ4_LIBRARIES ${LZ4_FULL_PATH})
    endif()
endif()
if(NOT LZ4_FOUND)
    find_path(LZ4_INCLUDE_DIRS lz4.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    find_library(LZ4_LIBRARIES
        NAMES lz4
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(LZ4_INCLUDE_DIRS AND LZ4_LIBRARIES)
        set(LZ4_FOUND TRUE)
    endif()
endif()

# Find xxHash
pkg_check_modules(XXHASH libxxhash)
if(XXHASH_FOUND AND NOT XXHASH_LIBRARIES MATCHES "^/")
    # pkg-config found it but only gave library name, find full path
    find_library(XXHASH_FULL_PATH
        NAMES ${XXHASH_LIBRARIES}
        PATHS ${XXHASH_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(XXHASH_FULL_PATH)
        set(XXHASH_LIBRARIES ${XXHASH_FULL_PATH})
    endif()
endif()
if(NOT XXHASH_FOUND)
    find_path(XXHASH_INCLUDE_DIRS xxhash.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    find_library(XXHASH_LIBRARIES
        NAMES xxhash
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(XXHASH_INCLUDE_DIRS AND XXHASH_LIBRARIES)
        set(XXHASH_FOUND TRUE)
    endif()
endif()

# Verify all dependencies are found
if(NOT ZMQ_FOUND)
    message(FATAL_ERROR "ZeroMQ not found. Please install libzmq.")
endif()
if(NOT LZ4_FOUND)
    message(FATAL_ERROR "LZ4 not found. Please install liblz4.")
endif()
if(NOT XXHASH_FOUND)
    message(FATAL_ERROR "xxHash not found. Please install libxxhash.")
endif()

# Optional: Find CAEN FELib if you want to build digitizer examples
find_library(CAEN_FELIB CAEN_FELib)

# ============================================================================
# BASIC EXAMPLES
# ============================================================================

# Example 1: Basic library usage
add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example 
    PRIVATE 
        ${DELILA_TARGET}
        ${ZMQ_LIBRARIES}
        ${LZ4_LIBRARIES}
        ${XXHASH_LIBRARIES}
)
target_include_directories(basic_example
    PRIVATE
        ${ZMQ_INCLUDE_DIRS}
        ${LZ4_INCLUDE_DIRS}
        ${XXHASH_INCLUDE_DIRS}
)

# More examples can be added here as needed
# For now, we only have basic_example.cpp

# ============================================================================
# INSTALLATION
# ============================================================================

# Install examples source code
install(FILES 
    CMakeLists.txt
    basic_example.cpp
    DESTINATION share/delila2/examples
)

# Install README if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    install(FILES README.md DESTINATION share/delila2/examples)
endif()

# Install built examples
install(TARGETS 
    basic_example
    DESTINATION bin
)

# ============================================================================
# BUILD INSTRUCTIONS
# ============================================================================

message(STATUS "")
message(STATUS "DELILA2 Examples Configuration:")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  DELILA2 found: ${DELILA2_FOUND}")
message(STATUS "  ZMQ: ${ZMQ_FOUND}")
if(ZMQ_FOUND)
    message(STATUS "    - Library: ${ZMQ_LIBRARIES}")
    message(STATUS "    - Include: ${ZMQ_INCLUDE_DIRS}")
endif()
message(STATUS "  LZ4: ${LZ4_FOUND}")
if(LZ4_FOUND)
    message(STATUS "    - Library: ${LZ4_LIBRARIES}")
    message(STATUS "    - Include: ${LZ4_INCLUDE_DIRS}")
endif()
message(STATUS "  xxHash: ${XXHASH_FOUND}")
if(XXHASH_FOUND)
    message(STATUS "    - Library: ${XXHASH_LIBRARIES}")
    message(STATUS "    - Include: ${XXHASH_INCLUDE_DIRS}")
endif()
message(STATUS "  CAEN FELib: ${CAEN_FELIB}")
message(STATUS "")
message(STATUS "To build examples:")
message(STATUS "  mkdir build")
message(STATUS "  cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "")