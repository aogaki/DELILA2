# Tests for DELILA2
cmake_minimum_required(VERSION 3.15)

# Try to find GoogleTest and benchmark (optional)
find_package(GTest QUIET)
find_package(benchmark QUIET)

# Only add tests if GoogleTest is found
if(GTest_FOUND)
    # Collect test source files
    file(GLOB_RECURSE UNIT_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
    )

    file(GLOB_RECURSE INTEGRATION_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp"
    )

    # Unit tests (if source files exist)
    if(UNIT_TEST_SOURCES)
        add_executable(delila_unit_tests ${UNIT_TEST_SOURCES})
        # Check if GMock is available
        if(TARGET GTest::gmock)
            target_link_libraries(delila_unit_tests
                DELILA
                GTest::gtest_main
                GTest::gmock
            )
        else()
            target_link_libraries(delila_unit_tests
                DELILA
                GTest::gtest_main
            )
        endif()
        add_test(NAME UnitTests COMMAND delila_unit_tests)
    endif()

    # Integration tests (if source files exist)
    if(INTEGRATION_TEST_SOURCES)
        add_executable(delila_integration_tests ${INTEGRATION_TEST_SOURCES})
        target_link_libraries(delila_integration_tests
            DELILA
            GTest::gtest_main
        )
        add_test(NAME IntegrationTests COMMAND delila_integration_tests)
    endif()
else()
    message(STATUS "GoogleTest not found - skipping test targets")
endif()

# Benchmarks (if benchmark is found and source files exist)
if(benchmark_FOUND)
    # Create separate executables for each benchmark to avoid symbol conflicts
    file(GLOB BENCHMARK_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
    )
    
    if(BENCHMARK_SOURCES)
        foreach(BENCHMARK_SOURCE ${BENCHMARK_SOURCES})
            get_filename_component(BENCHMARK_NAME ${BENCHMARK_SOURCE} NAME_WE)
            add_executable(${BENCHMARK_NAME} ${BENCHMARK_SOURCE})
            target_link_libraries(${BENCHMARK_NAME}
                DELILA
                benchmark::benchmark
            )
        endforeach()
    endif()
else()
    message(STATUS "Google Benchmark not found - skipping benchmark targets")
endif()