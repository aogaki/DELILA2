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

# Find Google Test for example tests (optional, not currently used)
# find_package(GTest QUIET)
# if(NOT GTest_FOUND)
#     # Try to find it manually
#     find_path(GTEST_INCLUDE_DIR gtest/gtest.h PATHS /opt/homebrew/include /usr/local/include /usr/include)
#     find_library(GTEST_LIBRARY gtest PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
#     find_library(GTEST_MAIN_LIBRARY gtest_main PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
#     
#     if(GTEST_INCLUDE_DIR AND GTEST_LIBRARY AND GTEST_MAIN_LIBRARY)
#         set(GTEST_FOUND TRUE)
#         set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
#         set(GTEST_LIBRARIES ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
#     endif()
# endif()

# Define all examples
set(EXAMPLES
    pubsub_publisher
    pubsub_subscriber
    push_pull_pusher
    push_pull_puller
    command_server
    command_client
    status_reporter
    status_monitor
    config_example
    throughput_test
    memory_pool_example
    robust_client
    simple_test
    test_command
    one_to_many_demo
    broadcast_vs_loadbalance
    filtered_broadcast
)

# Create executables for all examples
foreach(example ${EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_include_directories(${example} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${example} ${COMMON_LIBRARIES})
    set_target_properties(${example} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endforeach()

# Build tests if GTest is found
# Note: Example tests are not yet implemented
# if(GTEST_FOUND)
#     enable_testing()
#     
#     # TODO: Add example tests here
#     
#     message(STATUS "Example tests will be built")
# else()
#     message(STATUS "Google Test not found - example tests will not be built")
# endif()

# Installation
install(TARGETS ${EXAMPLES}
    RUNTIME DESTINATION bin
)

# Print configuration info
message(STATUS "DELILA2 Network Examples Configuration:")
message(STATUS "  ZMQ Found: ${ZMQ_FOUND}")
message(STATUS "  LZ4 Found: ${LZ4_FOUND}")
message(STATUS "  xxHash Found: ${XXHASH_FOUND}")
message(STATUS "  Net Library: ${DELILA_NET_LIBRARY}")