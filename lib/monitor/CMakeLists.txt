# Monitor library CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Collect source files
file(GLOB MONITOR_SOURCES "src/*.cpp")
file(GLOB MONITOR_HEADERS "include/*.hpp")

# Create monitor library as OBJECT (will be compiled into main DELILA library)
add_library(delila_monitor OBJECT 
    ${MONITOR_SOURCES}
    ${MONITOR_HEADERS}
)

target_include_directories(delila_monitor
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/delila/monitor>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add digitizer include path for EventData access
target_include_directories(delila_monitor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(delila_monitor PUBLIC cxx_std_17)

# Add compile options
target_compile_options(delila_monitor PRIVATE -pthread -O2)

# Monitor library tests
if(BUILD_TESTS AND GTest_FOUND)
    file(GLOB MONITOR_TEST_SOURCES "tests/*.cpp")
    
    if(MONITOR_TEST_SOURCES)
        add_executable(monitor_tests ${MONITOR_TEST_SOURCES})
        
        target_link_libraries(monitor_tests
            delila_monitor
            GTest::gtest_main
            GTest::gmock
            ${ROOT_LIBRARIES}
            pthread
        )
        
        target_include_directories(monitor_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/../digitizer/include
        )
        
        add_test(NAME MonitorTests COMMAND monitor_tests)
    endif()
endif()