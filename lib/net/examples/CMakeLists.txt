cmake_minimum_required(VERSION 3.15)
project(DELILA_Net_Examples VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Inherit all configuration from parent project - no independent detection needed
message(STATUS "Examples: Using parent ZMQ config: ${ZMQ_LIBRARIES}")
message(STATUS "Examples: Using parent LZ4 config: ${LZ4_LIBRARIES}")
message(STATUS "Examples: Using parent xxHash config: ${XXHASH_LIBRARIES}")

# Verify parent configured dependencies properly
if(NOT ZMQ_FOUND)
    message(FATAL_ERROR "ZMQ not found in parent project configuration")
endif()
if(NOT LZ4_FOUND)
    message(FATAL_ERROR "LZ4 not found in parent project configuration")  
endif()
if(NOT XXHASH_FOUND)
    message(FATAL_ERROR "xxHash not found in parent project configuration")
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