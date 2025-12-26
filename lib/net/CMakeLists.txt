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
add_library(delila_net OBJECT
    src/ZMQTransport.cpp
    src/EventData.cpp
    src/DataProcessor.cpp
    src/SequenceGapDetector.cpp
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
        ${ZMQ_INCLUDE_DIRS}
)

# Link ZeroMQ library
target_link_libraries(delila_net
    PUBLIC
        ${ZMQ_LIBRARIES}
)

# Find GTest
find_package(GTest REQUIRED)

# Enable testing if requested
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
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
    DESTINATION include/delila/net
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT DELILA_NetTargets
    FILE DELILA_NetTargets.cmake
    NAMESPACE DELILA::
    DESTINATION lib/cmake/DELILA_Net
)