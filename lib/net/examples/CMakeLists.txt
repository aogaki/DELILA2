cmake_minimum_required(VERSION 3.15)
project(DELILA_Net_Examples VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find ZeroMQ
find_package(PkgConfig REQUIRED)

# Direct path specification for ZMQ on macOS
if(EXISTS "/opt/homebrew/lib/libzmq.dylib")
    set(ZMQ_LIBRARIES "/opt/homebrew/lib/libzmq.dylib")
    set(ZMQ_INCLUDE_DIRS "/opt/homebrew/include")
    set(ZMQ_FOUND TRUE)
elseif(EXISTS "/usr/local/lib/libzmq.dylib")
    set(ZMQ_LIBRARIES "/usr/local/lib/libzmq.dylib")
    set(ZMQ_INCLUDE_DIRS "/usr/local/include")
    set(ZMQ_FOUND TRUE)
elseif(EXISTS "/usr/lib/libzmq.dylib")
    set(ZMQ_LIBRARIES "/usr/lib/libzmq.dylib")
    set(ZMQ_INCLUDE_DIRS "/usr/include")
    set(ZMQ_FOUND TRUE)
else()
    pkg_check_modules(ZMQ REQUIRED libzmq)
    
    # Alternative ZeroMQ setup if pkg-config fails
    if(NOT ZMQ_FOUND)
        find_path(ZMQ_INCLUDE_DIR zmq.h PATHS /opt/homebrew/include /usr/local/include /usr/include)
        find_library(ZMQ_LIBRARY zmq PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
        if(ZMQ_INCLUDE_DIR AND ZMQ_LIBRARY)
            set(ZMQ_FOUND TRUE)
            set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR})
            set(ZMQ_LIBRARIES ${ZMQ_LIBRARY})
        endif()
    endif()
endif()

# Find LZ4 - use same approach as main project
if(EXISTS "/opt/homebrew/lib/liblz4.dylib")
    set(LZ4_LIBRARIES "/opt/homebrew/lib/liblz4.dylib")
    set(LZ4_INCLUDE_DIRS "/opt/homebrew/include")
    set(LZ4_FOUND TRUE)
elseif(EXISTS "/usr/local/lib/liblz4.dylib")
    set(LZ4_LIBRARIES "/usr/local/lib/liblz4.dylib")
    set(LZ4_INCLUDE_DIRS "/usr/local/include")
    set(LZ4_FOUND TRUE)
elseif(EXISTS "/usr/lib/liblz4.dylib")
    set(LZ4_LIBRARIES "/usr/lib/liblz4.dylib")
    set(LZ4_INCLUDE_DIRS "/usr/include")
    set(LZ4_FOUND TRUE)
else()
    find_path(LZ4_INCLUDE_DIR lz4.h PATHS /opt/homebrew/include /usr/local/include /usr/include)
    find_library(LZ4_LIBRARY lz4 PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
    if(LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
        set(LZ4_FOUND TRUE)
        set(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})
        set(LZ4_LIBRARIES ${LZ4_LIBRARY})
    else()
        pkg_check_modules(LZ4 REQUIRED liblz4)
    endif()
endif()

# Find xxHash - use same approach as main project
if(EXISTS "/opt/homebrew/lib/libxxhash.dylib")
    set(XXHASH_LIBRARIES "/opt/homebrew/lib/libxxhash.dylib")
    set(XXHASH_INCLUDE_DIRS "/opt/homebrew/include")
    set(XXHASH_FOUND TRUE)
elseif(EXISTS "/usr/local/lib/libxxhash.dylib")
    set(XXHASH_LIBRARIES "/usr/local/lib/libxxhash.dylib")
    set(XXHASH_INCLUDE_DIRS "/usr/local/include")
    set(XXHASH_FOUND TRUE)
elseif(EXISTS "/usr/lib/libxxhash.dylib")
    set(XXHASH_LIBRARIES "/usr/lib/libxxhash.dylib")
    set(XXHASH_INCLUDE_DIRS "/usr/include")
    set(XXHASH_FOUND TRUE)
else()
    find_path(XXHASH_INCLUDE_DIR xxhash.h PATHS /opt/homebrew/include /usr/local/include /usr/include)
    find_library(XXHASH_LIBRARY xxhash PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
    if(XXHASH_INCLUDE_DIR AND XXHASH_LIBRARY)
        set(XXHASH_FOUND TRUE)
        set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIR})
        set(XXHASH_LIBRARIES ${XXHASH_LIBRARY})
    else()
        pkg_check_modules(XXHASH REQUIRED libxxhash)
    endif()
endif()

# Check if we're building as part of the main project
if(TARGET delila_net)
    # Building as part of main project
    set(DELILA_NET_LIBRARY delila_net)
    set(DELILA_NET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
else()
    # Building standalone - try to find library
    find_library(DELILA_NET_LIBRARY delila_net PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/../build
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../build/lib/net
    )
    
    if(NOT DELILA_NET_LIBRARY)
        message(FATAL_ERROR "Could not find delila_net library. Please build the lib/net project first.")
    endif()
    
    set(DELILA_NET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif()

# Common include directories and libraries
set(COMMON_INCLUDE_DIRS
    ${DELILA_NET_INCLUDE_DIR}
    ${LZ4_INCLUDE_DIRS}
    ${XXHASH_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
)

set(COMMON_LIBRARIES
    ${DELILA_NET_LIBRARY}
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