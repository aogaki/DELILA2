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
- Optional: ROOT, LZ4, xxHash

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

// Transport with automatic format selection
ZMQTransport transport;
transport.SendMinimal(events);

// Receive with format detection
auto data_type = transport.PeekDataType();
if (data_type == DataType::MINIMAL_EVENTDATA) {
    auto [events, seq] = transport.ReceiveMinimal();
}
```

## Documentation
- [System Architecture](DESIGN.md)
- [MinimalEventData Feature Summary](MinimalEventData_Feature_Summary.md)
- [Development Guidelines](CLAUDE.md)
- Implementation TODOs in `TODO/` directory

## License
See [LICENSE](LICENSE) file for details
