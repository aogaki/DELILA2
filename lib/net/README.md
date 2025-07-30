# DELILA2 Network Library Manual

## Overview

The DELILA2 Network Library provides high-performance data transport for nuclear physics data acquisition systems. It consists of two main components:

- **Serializer**: Binary serialization with LZ4 compression and xxHash validation
- **ZMQTransport**: ZeroMQ-based transport with PUB/SUB and REQ/REP patterns

## Quick Start

### Basic Dependencies
```bash
# macOS (Homebrew)
brew install zeromq lz4 xxhash

# Ubuntu/Debian
sudo apt-get install libzmq3-dev liblz4-dev libxxhash-dev

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Running Tests
```bash
# Run all tests
ctest --output-on-failure

# Run specific test
ctest -R test_serializer -V
```

## Core Components

### 1. EventData Structure

The fundamental data structure for nuclear physics events:

```cpp
#include "EventData.hpp"
using namespace DELILA::Digitizer;

// Create event
auto event = std::make_unique<EventData>();
event->timeStampNs = 1234567890;
event->energy = 1500;
event->module = 0;
event->channel = 3;

// Add waveform data
event->analogProbe1 = {100, 150, 200, 180, 120};
event->digitalProbe1 = {1, 1, 0, 1, 0};
```

### 2. Serializer

High-performance binary serialization with compression:

```cpp
#include "Serializer.hpp"
using namespace DELILA::Net;

Serializer serializer;

// Enable features
serializer.EnableCompression(true);
serializer.EnableChecksum(true);

// Serialize events
auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
events->push_back(std::move(event));

uint64_t sequence = 1;
auto encoded = serializer.Encode(events, sequence);

// Deserialize
auto [decoded_events, sequence_num] = serializer.Decode(encoded);
```

### 3. ZMQTransport

ZeroMQ-based transport layer:

```cpp
#include "ZMQTransport.hpp"
using namespace DELILA::Net;

// Configuration
TransportConfig config;
config.data_address = "tcp://localhost:5555";
config.is_publisher = true;  // or false for subscriber
config.bind_data = true;     // or false to connect

// Create and configure
auto transport = std::make_unique<ZMQTransport>();
transport->Configure(config);
transport->Connect();

// Send data
transport->Send(events);

// Receive data (on subscriber side)
auto [received_events, seq] = transport->Receive();
```

## Communication Patterns

### 1. Data Transport (PUB/SUB)

**Publisher (Sender)**:
```cpp
TransportConfig pub_config;
pub_config.data_address = "tcp://*:5555";
pub_config.is_publisher = true;
pub_config.bind_data = true;

auto publisher = std::make_unique<ZMQTransport>();
publisher->Configure(pub_config);
publisher->Connect();

// Send events
publisher->Send(events);
```

**Subscriber (Receiver)**:
```cpp
TransportConfig sub_config;
sub_config.data_address = "tcp://localhost:5555";
sub_config.is_publisher = false;
sub_config.bind_data = false;

auto subscriber = std::make_unique<ZMQTransport>();
subscriber->Configure(sub_config);
subscriber->Connect();

// Receive events
auto result = subscriber->Receive();
if (result.first) {
    std::cout << "Received " << result.first->size() << " events\n";
}
```

### 2. Status Communication (REQ/REP)

```cpp
// Status message
ComponentStatus status;
status.component_id = "digitizer_01";
status.state = "acquiring";
status.heartbeat_counter = 42;

// Send status
transport->SendStatus(status);

// Receive status
auto received_status = transport->ReceiveStatus();
```

### 3. Command Interface (REQ/REP)

```cpp
// Command message
Command cmd;
cmd.command_id = "cmd_001";
cmd.type = CommandType::START;
cmd.target_component = "digitizer";

// Send command and get response
auto response = transport->SendCommand(cmd);
if (response.success) {
    std::cout << "Command executed successfully\n";
}
```

## Configuration Options

### TransportConfig Structure

```cpp
struct TransportConfig {
    std::string data_address = "tcp://localhost:5555";
    std::string status_address = "tcp://localhost:5556";  
    std::string command_address = "tcp://localhost:5557";
    bool bind_data = true;
    bool bind_status = true;
    bool bind_command = false;
    bool is_publisher = true;  // Role: publisher or subscriber
};
```

### Serializer Options

```cpp
Serializer serializer;

// Enable/disable compression (default: enabled)
serializer.EnableCompression(true);

// Enable/disable checksum validation (default: enabled)
serializer.EnableChecksum(true);
```

## Performance Features

### Binary Format
- **Magic number**: 0xDEADBEEF for format validation
- **Version handling**: Forward/backward compatibility
- **Size optimization**: Efficient packing of EventData fields

### Compression
- **LZ4 algorithm**: Fast compression/decompression
- **Automatic selection**: Only compresses if beneficial
- **Waveform optimization**: Excellent for repetitive data

### Network Optimization
- **Non-blocking sends**: Prevents thread blocking
- **Configurable timeouts**: 1-second default for receives
- **Sequence numbering**: Data ordering and duplicate detection

## Error Handling

All methods use simple error patterns:

```cpp
// Boolean returns for operations
if (!transport->Configure(config)) {
    std::cerr << "Configuration failed\n";
}

// nullptr returns for failed operations
auto result = transport->Receive();
if (!result.first) {
    std::cerr << "No data received or error occurred\n";
}

// Response objects for command operations
auto response = transport->SendCommand(cmd);
if (!response.success) {
    std::cerr << "Command failed: " << response.message << "\n";
}
```

## Thread Safety

- **Serializer**: Thread-safe for read operations, serialize different data in parallel
- **ZMQTransport**: Not thread-safe, use one instance per thread or add external synchronization
- **EventData**: Thread-safe for read operations after construction

## Memory Management

The library uses modern C++ RAII principles:

- **Smart pointers**: Automatic memory management
- **Move semantics**: Efficient data transfer 
- **RAII**: Automatic resource cleanup
- **No memory leaks**: All resources properly released

## Troubleshooting

### Common Issues

1. **"Address already in use"**: Change port numbers or use different addresses
2. **"Connection refused"**: Ensure publisher is started before subscriber
3. **"No messages received"**: Check ZeroMQ "slow joiner" issue, add delays
4. **Build errors**: Ensure all dependencies are installed

### Debug Mode

```cpp
// Enable debug logging (implementation-specific)
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### Performance Monitoring

```cpp
// Measure serialization performance
auto start = std::chrono::high_resolution_clock::now();
auto encoded = serializer.Encode(events, sequence);
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Serialization took: " << duration.count() << " μs\n";
```

## Example Programs

See the `examples/` directory for complete toy programs:

- `sender.cpp`: Data publisher with random EventData generation
- `receiver.cpp`: Data subscriber with performance measurement
- `performance_test.cpp`: Comprehensive benchmarking suite

## API Reference

### EventData Fields

```cpp
uint64_t timeStampNs;           // Event timestamp in nanoseconds
uint32_t waveformSize;          // Total waveform samples
uint32_t energy;                // Pulse energy
uint32_t energyShort;           // Short gate energy
uint16_t module;                // Digitizer module ID
uint16_t channel;               // Channel number
uint16_t timeResolution;        // Time resolution
uint8_t analogProbe1Type;       // Analog probe 1 configuration
uint8_t analogProbe2Type;       // Analog probe 2 configuration
uint8_t digitalProbe1Type;      // Digital probe 1 configuration
uint8_t digitalProbe2Type;      // Digital probe 2 configuration
uint8_t digitalProbe3Type;      // Digital probe 3 configuration
uint8_t digitalProbe4Type;      // Digital probe 4 configuration
uint8_t downSampleFactor;       // Waveform downsampling
uint32_t flags;                 // Event flags (pileup, overflow, etc.)

// Waveform data
std::vector<int32_t> analogProbe1;
std::vector<int32_t> analogProbe2;
std::vector<uint8_t> digitalProbe1;
std::vector<uint8_t> digitalProbe2;
std::vector<uint8_t> digitalProbe3;
std::vector<uint8_t> digitalProbe4;
```

This manual covers the current implementation status. The library is production-ready for high-performance nuclear physics data acquisition systems.