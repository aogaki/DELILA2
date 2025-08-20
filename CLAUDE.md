# CLAUDE.md - Test-Driven Development Guide for DELILA2

This document provides guidelines for Claude to assist with Test-Driven Development (TDD) in the DELILA2 project.

## Project Overview

DELILA2 is a high-performance data acquisition system for nuclear physics experiments, written in C++17. The project consists of two main components:
- **Digitizer Library**: Hardware interface and data acquisition
- **Network Library**: High-performance data transport and serialization

## Build System

The project uses CMake (minimum version 3.15) as its build system with cross-platform support for Linux and macOS.

### Platform-Specific Setup

#### Linux Setup
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake pkg-config
sudo apt install libzmq3-dev liblz4-dev

# CentOS/RHEL/Rocky Linux  
sudo dnf install gcc-c++ cmake pkg-config
sudo dnf install zeromq-devel lz4-devel
```

#### macOS Setup
```bash
# Install dependencies via Homebrew
brew install cmake pkg-config
brew install zeromq lz4
```

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

## Test-Driven Development (TDD) Approach

When implementing new features (especially for the Network library with ZeroMQ), follow strict TDD methodology:

### TDD Red-Green-Refactor Cycle

1. **RED**: Write a failing test first
   ```cpp
   TEST(ZMQTransportTest, ShouldConnectToValidAddress) {
       ZMQTransport transport;
       TransportConfig config;
       config.data_channel.address = "tcp://localhost:5555";
       config.data_channel.pattern = "PUB";
       config.data_channel.bind = true;
       
       EXPECT_TRUE(transport.Configure(config));
       EXPECT_TRUE(transport.Connect());
       EXPECT_TRUE(transport.IsConnected());
   }
   ```

2. **GREEN**: Write minimal code to make test pass
   ```cpp
   bool ZMQTransport::Configure(const TransportConfig& config) {
       // Minimal implementation
       return true;  // Just enough to pass
   }
   ```

3. **REFACTOR**: Improve code while keeping tests green
   ```cpp
   bool ZMQTransport::Configure(const TransportConfig& config) {
       // Proper validation and setup
       if (config.data_channel.address.empty()) return false;
       fConfig = config;
       return true;
   }
   ```

### TDD Guidelines for ZeroMQ Implementation

#### Start with Simple Tests
```cpp
// Test 1: Basic construction
TEST(ZMQTransportTest, ConstructorCreatesValidInstance) {
    ZMQTransport transport;
    EXPECT_FALSE(transport.IsConnected());
}

// Test 2: Configuration validation
TEST(ZMQTransportTest, RejectsInvalidConfiguration) {
    ZMQTransport transport;
    TransportConfig invalid_config;  // Empty config
    EXPECT_FALSE(transport.Configure(invalid_config));
}

// Test 3: Basic connection
TEST(ZMQTransportTest, ConnectsWithValidConfig) {
    // Implementation as shown above
}
```

#### Progress to Complex Scenarios
```cpp
// Test data flow
TEST(ZMQTransportTest, SendReceiveEventData) {
    // Setup publisher and subscriber
    // Send EventData through Serializer
    // Verify received data matches sent data
}

// Test error conditions
TEST(ZMQTransportTest, HandlesConnectionFailure) {
    // Test with unreachable address
    // Verify proper error handling
}
```

## KISS Principle (Keep It Simple, Stupid)

Apply KISS ruthlessly to avoid over-engineering the ZeroMQ wrapper:

### Simple Interface Design
```cpp
// GOOD: Simple, focused interface
class ZMQTransport {
public:
    bool Configure(const TransportConfig& config);
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    
    // Data functions
    bool Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events);
    std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> Receive();
    
    // Status functions  
    bool SendStatus(const ComponentStatus& status);
    std::unique_ptr<ComponentStatus> ReceiveStatus();
    
    // Command functions
    CommandResponse SendCommand(const Command& command);
    std::unique_ptr<Command> ReceiveCommand();
};

// BAD: Over-engineered with too many options
class ComplexZMQTransport {
    // 50+ methods with multiple overloads
    // Complex template-based configuration
    // Multiple inheritance hierarchies
};
```

### Simple Implementation Strategy

#### Phase 1: Core Functionality Only
- Basic PUB/SUB for data transport
- Simple JSON for status/commands
- TCP transport only
- No advanced features

#### Phase 2: Essential Features
- Add REQ/REP for commands
- Error handling and reconnection
- Basic configuration validation

#### Phase 3: Polish (Only if needed)
- Performance optimizations
- Additional transport protocols
- Advanced error recovery

### KISS Testing Strategy

#### Simple Test Structure
```cpp
class ZMQTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Minimal setup
        transport = std::make_unique<ZMQTransport>();
    }
    
    std::unique_ptr<ZMQTransport> transport;
};

// One concept per test
TEST_F(ZMQTransportTest, ConfigurationWorks) { /* ... */ }
TEST_F(ZMQTransportTest, ConnectionWorks) { /* ... */ }
TEST_F(ZMQTransportTest, DataSendWorks) { /* ... */ }
```

#### Avoid Complex Test Setups
```cpp
// GOOD: Simple, direct test
TEST_F(ZMQTransportTest, SendsEventData) {
    auto events = CreateTestEvents(1);
    transport->Configure(GetBasicConfig());
    transport->Connect();
    
    EXPECT_TRUE(transport->Send(events));
}

// BAD: Complex setup with multiple dependencies
TEST_F(ZMQTransportTest, ComplexIntegrationScenario) {
    // 50 lines of setup
    // Multiple mock objects
    // Complex state manipulation
    // Hard to understand what's being tested
}
```

### KISS Configuration

#### Simple Configuration Object
```cpp
struct TransportConfig {
    std::string data_address = "tcp://localhost:5555";
    std::string status_address = "tcp://localhost:5556";  
    std::string command_address = "tcp://localhost:5557";
    bool bind_data = true;
    bool bind_status = true;
    bool bind_command = false;
    
    // That's it - no complex nested structures initially
};
```

### KISS Error Handling

#### Simple Error Strategy
```cpp
// Return bool for success/failure
bool Send(const EventData& data) {
    try {
        // ZMQ operation
        return true;
    } catch (...) {
        // Log error
        return false;
    }
}

// Use optional for data that might not exist
std::optional<ComponentStatus> ReceiveStatus() {
    // Simple receive logic
    // Return empty optional on failure/timeout
}
```

### KISS Development Rules

1. **One Feature at a Time**: Implement and test one function completely before moving to the next
2. **No Premature Optimization**: Get it working first, optimize later if needed
3. **Minimal Dependencies**: Use only what's absolutely necessary
4. **Clear Names**: Function and variable names should be immediately understandable
5. **Short Functions**: Keep functions under 20 lines when possible
6. **No Magic Numbers**: Use named constants for all configuration values
7. **Fail Fast**: Validate inputs early and return clear error indicators

### TDD + KISS Integration

```cpp
// Write simple test first
TEST_F(ZMQTransportTest, SendsData) {
    EXPECT_TRUE(transport->Send(CreateTestData()));
}

// Implement simplest possible solution
bool ZMQTransport::Send(const std::vector<uint8_t>& data) {
    return true;  // Fake it till you make it
}

// Add real implementation incrementally
bool ZMQTransport::Send(const std::vector<uint8_t>& data) {
    if (!IsConnected()) return false;
    
    zmq::message_t msg(data.size());
    memcpy(msg.data(), data.data(), data.size());
    
    try {
        return fSocket.send(msg, zmq::send_flags::dontwait);
    } catch (...) {
        return false;
    }
}
```

Remember: Good tests are the foundation of reliable software. Write them first, keep them simple, and maintain them alongside your code. When in doubt, choose the simpler solution - you can always add complexity later if truly needed.