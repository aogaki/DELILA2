# CMakeLists.txt for DELILA2 Binary Serialization Tests
cmake_minimum_required(VERSION 3.15)

# Find GoogleTest
find_package(GTest REQUIRED)

# Find Google Benchmark
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BENCHMARK QUIET benchmark)
    if(BENCHMARK_FOUND)
        message(STATUS "Found Google Benchmark via pkg-config")
        add_library(benchmark::benchmark INTERFACE IMPORTED)
        target_link_libraries(benchmark::benchmark INTERFACE ${BENCHMARK_LIBRARIES})
        target_include_directories(benchmark::benchmark INTERFACE ${BENCHMARK_INCLUDE_DIRS})
        set(benchmark_FOUND TRUE)
    endif()
endif()

# Enable testing
enable_testing()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Unit Tests Executable
set(UNIT_TEST_SOURCES
    unit/utils/test_error.cpp
    unit/utils/test_platform.cpp
    unit/serialization/test_protocol_constants.cpp
    unit/serialization/test_binary_serializer.cpp
    unit/test/test_data_generator.cpp
    # More test files will be added as we implement them
)

add_executable(delila_net_binary_unit_tests ${UNIT_TEST_SOURCES})

target_link_libraries(delila_net_binary_unit_tests 
    GTest::gtest_main
    GTest::gmock
    delila_net_binary  # Our library (will be created)
)

target_include_directories(delila_net_binary_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add test to CTest
add_test(NAME UnitTests COMMAND delila_net_binary_unit_tests)
set_tests_properties(UnitTests PROPERTIES
    TIMEOUT 300
    LABELS "unit"
)

# Integration Tests Executable
set(INTEGRATION_TEST_SOURCES
    integration/test_end_to_end.cpp
)

add_executable(delila_net_binary_integration_tests ${INTEGRATION_TEST_SOURCES})

target_link_libraries(delila_net_binary_integration_tests 
    GTest::gtest_main
    GTest::gmock
    delila_net_binary
)

target_include_directories(delila_net_binary_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add integration test to CTest
add_test(NAME IntegrationTests COMMAND delila_net_binary_integration_tests)
set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 600
    LABELS "integration"
    DEPENDS UnitTests
)

# Benchmark Tests (if Google Benchmark is available)
if(benchmark_FOUND)
    set(BENCHMARK_SOURCES
        benchmarks/bench_serialization.cpp
        # More benchmark files will be added later
    )
    
    set(COMPREHENSIVE_BENCHMARK_SOURCES
        benchmarks/bench_comprehensive.cpp
    )
    
    add_executable(delila_net_binary_benchmarks ${BENCHMARK_SOURCES})
    
    target_link_libraries(delila_net_binary_benchmarks
        benchmark::benchmark
        delila_net_binary
    )
    
    target_include_directories(delila_net_binary_benchmarks PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Comprehensive benchmarks for Phase 7
    add_executable(delila_net_binary_comprehensive_benchmarks ${COMPREHENSIVE_BENCHMARK_SOURCES})
    
    target_link_libraries(delila_net_binary_comprehensive_benchmarks
        benchmark::benchmark
        delila_net_binary
    )
    
    target_include_directories(delila_net_binary_comprehensive_benchmarks PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Add benchmark test
    add_test(NAME Benchmarks COMMAND delila_net_binary_benchmarks 
        --benchmark_min_time=1s
        --benchmark_filter=".*"
    )
    set_tests_properties(Benchmarks PROPERTIES
        TIMEOUT 600
        LABELS "benchmark"
        DEPENDS UnitTests
    )
    
    # Add comprehensive benchmark test for Phase 7
    add_test(NAME ComprehensiveBenchmarks COMMAND delila_net_binary_comprehensive_benchmarks
        --benchmark_min_time=2s
        --benchmark_filter=".*"
    )
    set_tests_properties(ComprehensiveBenchmarks PROPERTIES
        TIMEOUT 1200
        LABELS "comprehensive_benchmark"
        DEPENDS UnitTests
    )
else()
    message(STATUS "Google Benchmark not found, skipping benchmark tests")
endif()

# Custom target for running tests sequentially
add_custom_target(test_sequential
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure
    DEPENDS delila_net_binary_unit_tests
    COMMENT "Running all tests sequentially"
)