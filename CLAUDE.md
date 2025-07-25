# CLAUDE.md - Test-Driven Development Guide for DELILA2

This document provides guidelines for Claude to assist with Test-Driven Development (TDD) in the DELILA2 project.

## Project Overview

DELILA2 is a high-performance data acquisition system for nuclear physics experiments, written in C++17. The project consists of two main components:
- **Digitizer Library**: Hardware interface and data acquisition
- **Network Library**: High-performance data transport and serialization

## Build System

The project uses CMake (minimum version 3.15) as its build system.

### Build Commands
```bash
# Create build directory
mkdir build && cd build

# Configure (Debug mode for testing)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build
make -j$(nproc)

# Run all tests
ctest --output-on-failure

# Run specific test
ctest -R <test_name> --output-on-failure

# Verbose test output
ctest -V
```

## Testing Framework

The project uses **Google Test (GTest)** for unit testing and **Google Benchmark** for performance testing.

### Test Organization
```
/tests/
├── CMakeLists.txt
├── unit/           # Unit tests
├── integration/    # Integration tests
└── benchmarks/     # Performance benchmarks

/lib/net/tests/
├── unit/
│   ├── config/
│   ├── serialization/
│   ├── transport/
│   └── utils/
├── integration/
└── benchmarks/

/lib/digitizer/
├── test_ch_extras_opt.cpp
├── test_dig1_init.cpp
└── test_timestamp_decoding.cpp
```

## TDD Workflow

When implementing new features or fixing bugs, follow this TDD workflow:

### 1. Write Test First
```cpp
// Example: tests/unit/test_feature.cpp
#include <gtest/gtest.h>
#include "delila/feature.h"

TEST(FeatureTest, ShouldReturnExpectedValue) {
    // Arrange
    Feature feature;
    
    // Act
    auto result = feature.process(42);
    
    // Assert
    EXPECT_EQ(result, 84);
}
```

### 2. Run Test (Expect Failure)
```bash
cd build
make test_feature
ctest -R FeatureTest --output-on-failure
```

### 3. Implement Minimal Code
Write just enough code to make the test pass.

### 4. Run Test (Expect Success)
```bash
make test_feature && ctest -R FeatureTest
```

### 5. Refactor
Improve code quality while keeping tests green.

## Testing Guidelines

### Unit Test Structure
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>  // For mocking

// Test fixture for complex setup
class DigitzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(DigitzerTest, InitializationSucceeds) {
    // Test implementation
}
```

### Integration Test Example
```cpp
// tests/integration/test_data_flow.cpp
TEST(DataFlowTest, EndToEndDataTransfer) {
    // Test complete data flow from digitizer to network
}
```

### Performance Benchmark Example
```cpp
// tests/benchmarks/bench_serialization.cpp
#include <benchmark/benchmark.h>

static void BM_Serialization(benchmark::State& state) {
    for (auto _ : state) {
        // Code to benchmark
    }
}
BENCHMARK(BM_Serialization);
```

## Common Test Patterns

### Testing with Dependencies
```cpp
// Use dependency injection
class DataProcessor {
    std::unique_ptr<ITransport> transport_;
public:
    explicit DataProcessor(std::unique_ptr<ITransport> transport)
        : transport_(std::move(transport)) {}
};

// In tests, inject mock
TEST(DataProcessorTest, ProcessesDataCorrectly) {
    auto mockTransport = std::make_unique<MockTransport>();
    DataProcessor processor(std::move(mockTransport));
    // Test logic
}
```

### Testing Async Operations
```cpp
TEST(AsyncTest, CompletesWithinTimeout) {
    std::promise<bool> promise;
    auto future = promise.get_future();
    
    // Start async operation
    asyncOperation([&promise]() {
        promise.set_value(true);
    });
    
    // Wait with timeout
    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), 
              std::future_status::ready);
    EXPECT_TRUE(future.get());
}
```

## Network Library Testing

For network components, use the provided test scripts:
```bash
# Run network tests
./PreSearch/net/scripts/run_test.sh

# Comprehensive network tests
./PreSearch/net/scripts/run_comprehensive_test.sh
```

## Code Coverage

To enable code coverage (when available):
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
make
make test
make coverage
```

## Continuous Testing

### Pre-commit Checks
Before committing, always run:
```bash
# Build and test
make -j$(nproc) && ctest --output-on-failure

# Check for memory issues (if valgrind available)
ctest -T memcheck
```

### Adding New Tests
1. Create test file in appropriate directory
2. Update CMakeLists.txt to include new test
3. Follow naming convention: `test_<component>_<feature>.cpp`

## Dependencies for Testing

Ensure these are installed:
- Google Test (automatically fetched by CMake if not found)
- Google Benchmark (optional, for performance tests)
- Valgrind (optional, for memory testing)

## Test Data Management

- Small test data: Include directly in test files
- Large test data: Place in `tests/data/` directory
- Binary test data: Use Git LFS if needed

## Debugging Tests

```bash
# Run test with gdb
gdb ./tests/unit/test_feature
(gdb) run

# Run with verbose output
ctest -V -R FeatureTest

# Run with specific GTest filter
./tests/unit/test_feature --gtest_filter=FeatureTest.SpecificTest
```

## Important Notes

1. **Always write tests before implementation** (TDD principle)
2. **Keep tests independent** - each test should be able to run in isolation
3. **Use descriptive test names** that explain what is being tested
4. **Test edge cases** and error conditions, not just happy paths
5. **Mock external dependencies** to keep tests fast and reliable
6. **Aim for high test coverage** but focus on meaningful tests
7. **Run tests frequently** during development

## Project-Specific Testing Considerations

### Digitizer Library
- Mock hardware interfaces for unit tests
- Use integration tests for actual hardware (when available)
- Test timestamp decoding accuracy
- Verify data integrity through acquisition pipeline

### Network Library
- Test serialization/deserialization correctness
- Verify network transport reliability
- Benchmark throughput and latency
- Test connection handling and reconnection logic

## Common Commands Summary

```bash
# Build with tests
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..
make -j$(nproc)

# Run all tests
ctest --output-on-failure

# Run specific test suite
ctest -R "DigitzerTest" -V

# Run benchmarks
./tests/benchmarks/bench_all

# Check test coverage (if configured)
make coverage

# Clean and rebuild
make clean && make -j$(nproc)
```

Remember: Good tests are the foundation of reliable software. Write them first, keep them simple, and maintain them alongside your code.