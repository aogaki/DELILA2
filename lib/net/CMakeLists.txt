cmake_minimum_required(VERSION 3.15)
project(DELILA_Net VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ZMQ configuration is inherited from parent project
# Verify ZMQ was found in parent configuration
if(NOT ZMQ_FOUND)
    message(FATAL_ERROR "ZMQ not found - should be configured in parent project")
endif()
message(STATUS "Net library using parent ZMQ config: ${ZMQ_LIBRARIES}")

# Create the library
add_library(delila_net
    src/Serializer.cpp
    src/ZMQTransport.cpp
    src/EventData.cpp
)

# Add include directories
target_include_directories(delila_net
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set target properties
set_target_properties(delila_net PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add digitizer include path for EventData.hpp
target_include_directories(delila_net
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include>
        $<INSTALL_INTERFACE:include>
        ${LZ4_INCLUDE_DIRS}
        ${ZMQ_INCLUDE_DIRS}
)

# Link LZ4, xxHash, and ZeroMQ libraries
target_link_libraries(delila_net
    PUBLIC
        ${LZ4_LIBRARIES}
        ${XXHASH_LIBRARIES}
        ${ZMQ_LIBRARIES}
)

# Find GTest
find_package(GTest REQUIRED)

# Create test executable
# TODO: test_serializer.cpp is missing
# add_executable(test_serializer test_serializer.cpp)
# target_link_libraries(test_serializer 
#     delila_net
#     delila_digitizer
#     GTest::gtest
#     GTest::gtest_main
#     ${LZ4_LIBRARIES}
#     ${XXHASH_LIBRARIES}
# )
# target_include_directories(test_serializer PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include
# )

# Create ZMQ transport test executable
# TODO: test_zmq_transport.cpp is missing
# add_executable(test_zmq_transport test_zmq_transport.cpp)
# target_link_libraries(test_zmq_transport 
#     delila_net
#     delila_digitizer
#     GTest::gtest
#     GTest::gtest_main
#     ${LZ4_LIBRARIES}
#     ${XXHASH_LIBRARIES}
#     ${ZMQ_LIBRARIES}
# )
# target_include_directories(test_zmq_transport PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include
#     ${ZMQ_INCLUDE_DIRS}
# )

# Create simple pattern test executable (no digitizer dependency)
# TODO: test_patterns_only.cpp is missing
# add_executable(test_patterns_only test_patterns_only.cpp)

# Create Phase 1 integration test executable
# NOTE: Commented out until test file is created
# add_executable(test_zmq_transport_phase1_integration test_zmq_transport_phase1_integration.cpp)
# target_link_libraries(test_zmq_transport_phase1_integration 
#     delila_net
#     delila_digitizer
#     GTest::gtest
#     GTest::gtest_main
#     ${LZ4_LIBRARIES}
#     ${XXHASH_LIBRARIES}
#     ${ZMQ_LIBRARIES}
# )
# target_include_directories(test_zmq_transport_phase1_integration PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include
#     ${ZMQ_INCLUDE_DIRS}
# )

# target_link_libraries(test_patterns_only 
#     delila_net
#     GTest::gtest
#     GTest::gtest_main
#     ${LZ4_LIBRARIES}
#     ${XXHASH_LIBRARIES}
#     ${ZMQ_LIBRARIES}
# )
# target_include_directories(test_patterns_only PRIVATE
#     ${ZMQ_INCLUDE_DIRS}
# )

# Enable testing if requested
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    # TODO: Uncomment when test files exist
    # add_test(NAME test_serializer COMMAND test_serializer)
    # add_test(NAME test_zmq_transport COMMAND test_zmq_transport)
    # add_test(NAME test_patterns_only COMMAND test_patterns_only)
    add_test(NAME test_zmq_transport_phase1_integration COMMAND test_zmq_transport_phase1_integration)
endif()

# Add examples subdirectory
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation rules
install(TARGETS delila_net
    EXPORT DELILA_NetTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT DELILA_NetTargets
    FILE DELILA_NetTargets.cmake
    NAMESPACE DELILA::
    DESTINATION lib/cmake/DELILA_Net
)