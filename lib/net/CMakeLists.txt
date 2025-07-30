cmake_minimum_required(VERSION 3.15)
project(DELILA_Net VERSION 1.0.0 LANGUAGES CXX)

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

# Create the library
add_library(delila_net
    src/Serializer.cpp
    src/ZMQTransport.cpp
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
add_executable(test_serializer test_serializer.cpp)
target_link_libraries(test_serializer 
    delila_net
    delila_digitizer
    GTest::gtest
    GTest::gtest_main
    ${LZ4_LIBRARIES}
    ${XXHASH_LIBRARIES}
)
target_include_directories(test_serializer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include
)

# Create ZMQ transport test executable
add_executable(test_zmq_transport test_zmq_transport.cpp)
target_link_libraries(test_zmq_transport 
    delila_net
    delila_digitizer
    GTest::gtest
    GTest::gtest_main
    ${LZ4_LIBRARIES}
    ${XXHASH_LIBRARIES}
    ${ZMQ_LIBRARIES}
)
target_include_directories(test_zmq_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include
    ${ZMQ_INCLUDE_DIRS}
)

# Enable testing if requested
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_test(NAME test_serializer COMMAND test_serializer)
    add_test(NAME test_zmq_transport COMMAND test_zmq_transport)
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