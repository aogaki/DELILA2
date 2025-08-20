# DELILA2
A high-performance data acquisition system for nuclear physics experiments

## Overview
DELILA2 is a modern, modular data acquisition framework designed for high-throughput nuclear physics experiments. It provides efficient data transport, serialization, and hardware integration with support for both full-featured EventData and memory-optimized MinimalEventData formats.

## Key Features
- **Dual Data Format Support**: 
  - EventData: Full-featured with waveform support (500+ bytes/event)
  - MinimalEventData: Memory-optimized POD structure (22 bytes/event, 96% reduction)
- **High Performance**: 128+ million events/second encoding, 73+ million events/second decoding
- **Network Transport**: ZeroMQ-based with automatic format detection
- **Modular Architecture**: Separate digitizer and network libraries
- **Comprehensive Testing**: TDD methodology with 100% test coverage

## Building DELILA2

### Prerequisites
- CMake 3.15 or higher
- C++17 compatible compiler
- ZeroMQ library
- Google Test (for testing)
- Optional: ROOT, LZ4

### Build Instructions
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Running Tests
```bash
# All tests
ctest --output-on-failure

# Specific test suites
./tests/delila_unit_tests
./tests/delila_integration_tests
./tests/benchmarks/delila_benchmarks
```

## MinimalEventData Feature

The MinimalEventData structure provides a memory-efficient alternative to the full EventData format, achieving 96% memory reduction while maintaining high performance. This is ideal for high-frequency data acquisition scenarios where memory bandwidth is critical.

### Features
- 22-byte packed POD structure
- Automatic format detection in transport layer
- Full backward compatibility with EventData
- 128M+ events/second encoding speed

### Usage Example
```cpp
// Create MinimalEventData
auto event = std::make_unique<MinimalEventData>(
    module, channel, timeStampNs, energy, energyShort, flags
);

// NEW API: Separated serialization (recommended)
ZMQTransport transport;
Serializer serializer;
uint64_t sequence = 1;

auto serialized_bytes = serializer.Encode(events, sequence);
transport.SendBytes(serialized_bytes);

// Receive and deserialize
auto received_bytes = transport.ReceiveBytes();
auto [events, seq] = serializer.Decode(received_bytes);

// OLD API: Integrated serialization (deprecated)
transport.SendMinimal(events);  // Still works, shows deprecation warning
auto [events, seq] = transport.ReceiveMinimal();
```

## Refactored Architecture (v2.0+)

DELILA2 has been refactored to separate transport and serialization concerns, following the Single Responsibility Principle and improving modularity.

### Key Changes
- **Separated Serialization**: Transport layer now handles only raw bytes
- **User-Controlled Serialization**: Applications manage serialization externally  
- **Zero-Copy Optimization**: Ownership transfer with `std::unique_ptr<std::vector<uint8_t>>`
- **Backward Compatibility**: Old API remains functional with deprecation warnings

### New Architecture Benefits
- ✅ **Better Testability**: Transport and serialization can be tested independently
- ✅ **Improved Modularity**: Clear separation of concerns
- ✅ **Enhanced Flexibility**: Multiple serializers, custom sequence management
- ✅ **Performance Maintained**: No regression vs old API (validated by benchmarks)

### Migration Guide

#### For New Code (Recommended)
```cpp
// 1. Setup transport and serializer separately
ZMQTransport transport;
Serializer serializer;

// 2. Configure transport (unchanged)
TransportConfig config;
config.data_address = "tcp://localhost:5555";
transport.Configure(config);
transport.Connect();

// 3. Serialize externally, then send bytes
auto events = CreateEvents();
auto bytes = serializer.Encode(events, sequence_number++);
transport.SendBytes(bytes);

// 4. Receive bytes, then deserialize externally  
auto received_bytes = transport.ReceiveBytes();
auto [received_events, seq] = serializer.Decode(received_bytes);
```

#### For Existing Code (Migration Path)
```cpp
// OLD API still works - migrate gradually
transport.Send(events);      // Shows deprecation warning
transport.SendMinimal(events); // Shows deprecation warning

// Replace with NEW API when convenient
auto bytes = serializer.Encode(events, seq);
transport.SendBytes(bytes);
```

### Examples
- **[Separated Serialization](examples/separated_serialization.cpp)**: Demonstrates new API
- **[MinimalEventData](examples/minimal_event_data_example.cpp)**: Shows both old and new approaches
- **[Data+Command](examples/data_command_example.cpp)**: Complete application example

## Documentation
- [System Architecture](DESIGN.md)
- [MinimalEventData Feature Summary](MinimalEventData_Feature_Summary.md)
- [Development Guidelines](CLAUDE.md)
- [Performance Validation](PERFORMANCE_VALIDATION.md)
- [API Interoperability](INTEROPERABILITY_VALIDATION.md)
- [Refactoring Tasks](REFACTORING_TASKS.md)

## License
See [LICENSE](LICENSE) file for details
