cmake_minimum_required(VERSION 3.15)
project(DELILA_Net_Examples VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find ZeroMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Alternative ZeroMQ setup for macOS/Homebrew
if(NOT ZMQ_FOUND)
    find_path(ZMQ_INCLUDE_DIR zmq.h PATHS /opt/homebrew/include /usr/local/include)
    find_library(ZMQ_LIBRARY zmq PATHS /opt/homebrew/lib /usr/local/lib)
    if(ZMQ_INCLUDE_DIR AND ZMQ_LIBRARY)
        set(ZMQ_FOUND TRUE)
        set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR})
        set(ZMQ_LIBRARIES ${ZMQ_LIBRARY})
    endif()
endif()

# Find LZ4 - use same approach as main project
find_path(LZ4_INCLUDE_DIR lz4.h PATHS /opt/homebrew/include /usr/local/include)
find_library(LZ4_LIBRARY lz4 PATHS /opt/homebrew/lib /usr/local/lib)
if(LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
    set(LZ4_FOUND TRUE)
    set(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})
    set(LZ4_LIBRARIES ${LZ4_LIBRARY})
else()
    pkg_check_modules(LZ4 REQUIRED liblz4)
endif()

# Find xxHash - use same approach as main project
find_path(XXHASH_INCLUDE_DIR xxhash.h PATHS /opt/homebrew/include /usr/local/include)
find_library(XXHASH_LIBRARY xxhash PATHS /opt/homebrew/lib /usr/local/lib)
if(XXHASH_INCLUDE_DIR AND XXHASH_LIBRARY)
    set(XXHASH_FOUND TRUE)
    set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIR})
    set(XXHASH_LIBRARIES ${XXHASH_LIBRARY})
else()
    pkg_check_modules(XXHASH REQUIRED libxxhash)
endif()

# Check if we're building as part of the main project
if(TARGET delila_net AND TARGET delila_digitizer)
    # Building as part of main project
    set(DELILA_NET_LIBRARY delila_net)
    set(DELILA_DIGITIZER_LIBRARY delila_digitizer)
    set(DELILA_NET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    set(DELILA_DIGITIZER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../digitizer/include)
else()
    # Building standalone - try to find libraries
    find_library(DELILA_NET_LIBRARY delila_net PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../build)
    find_library(DELILA_DIGITIZER_LIBRARY delila_digitizer PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../digitizer/build)
    
    if(NOT DELILA_NET_LIBRARY OR NOT DELILA_DIGITIZER_LIBRARY)
        message(FATAL_ERROR "Could not find DELILA libraries. Please build the main project first.")
    endif()
    
    set(DELILA_NET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    set(DELILA_DIGITIZER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../digitizer/include)
endif()

# Common include directories and libraries
set(COMMON_INCLUDE_DIRS
    ${DELILA_NET_INCLUDE_DIR}
    ${DELILA_DIGITIZER_INCLUDE_DIR}
    ${LZ4_INCLUDE_DIRS}
    ${XXHASH_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
)

set(COMMON_LIBRARIES
    ${DELILA_NET_LIBRARY}
    ${DELILA_DIGITIZER_LIBRARY}
    ${LZ4_LIBRARIES}
    ${XXHASH_LIBRARIES}
    ${ZMQ_LIBRARIES}
)

# Sender executable
add_executable(sender sender.cpp)
target_include_directories(sender PRIVATE ${COMMON_INCLUDE_DIRS})
target_link_libraries(sender ${COMMON_LIBRARIES})

# Receiver executable
add_executable(receiver receiver.cpp)
target_include_directories(receiver PRIVATE ${COMMON_INCLUDE_DIRS})
target_link_libraries(receiver ${COMMON_LIBRARIES})

# Performance test executable
add_executable(performance_test performance_test.cpp)
target_include_directories(performance_test PRIVATE ${COMMON_INCLUDE_DIRS})
target_link_libraries(performance_test ${COMMON_LIBRARIES})

# Set target properties
set_target_properties(sender receiver performance_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Installation (optional)
install(TARGETS sender receiver performance_test
    RUNTIME DESTINATION bin
)

# Print configuration info
message(STATUS "DELILA2 Network Examples Configuration:")
message(STATUS "  ZMQ Found: ${ZMQ_FOUND}")
message(STATUS "  LZ4 Found: ${LZ4_FOUND}")
message(STATUS "  xxHash Found: ${XXHASH_FOUND}")
message(STATUS "  Net Library: ${DELILA_NET_LIBRARY}")
message(STATUS "  Digitizer Library: ${DELILA_DIGITIZER_LIBRARY}")