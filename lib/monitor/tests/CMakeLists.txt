# Simple CMakeLists.txt for testing Recorder
cmake_minimum_required(VERSION 3.15)
project(RecorderTests)

# Find required packages
find_package(GTest REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core Tree RIO)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../digitizer/include
    ${ROOT_INCLUDE_DIRS}
)

# Create test executables
add_executable(test_recorder test_recorder.cpp ../src/Recorder.cpp ../../digitizer/src/EventData.cpp)

# Link libraries
target_link_libraries(test_recorder
    GTest::gtest_main
    ${ROOT_LIBRARIES}
    pthread
)

# Set C++ standard
set_target_properties(test_recorder PROPERTIES CXX_STANDARD 17)

# Add tests
enable_testing()
add_test(NAME RecorderTest COMMAND test_recorder)