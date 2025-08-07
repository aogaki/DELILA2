# MinimalEventData API Documentation

## Overview
MinimalEventData is a memory-efficient data structure designed for high-throughput data acquisition in DELILA2. It provides a compact 22-byte representation of physics events while maintaining essential information.

## Header File
```cpp
#include "delila/core/MinimalEventData.hpp"
```

## Class Definition

### MinimalEventData Structure
```cpp
struct MinimalEventData {
    uint8_t module;        // Module ID (0-255)
    uint8_t channel;       // Channel ID (0-255)
    uint16_t padding;      // Alignment padding (unused)
    uint32_t padding2;     // Alignment padding (unused)
    double timeStampNs;    // Timestamp in nanoseconds
    uint16_t energy;       // Energy value
    uint16_t energyShort;  // Short gate energy
    uint32_t flags;        // Event flags (reduced from 64-bit)
};
```

## Constructors

### Default Constructor
```cpp
MinimalEventData()
```
Creates a MinimalEventData object with all fields initialized to zero.

### Parameterized Constructor
```cpp
MinimalEventData(uint8_t module, 
                 uint8_t channel, 
                 double timeStampNs,
                 uint16_t energy, 
                 uint16_t energyShort, 
                 uint64_t flags)
```
Creates a MinimalEventData object with specified values.

**Parameters:**
- `module`: Module identifier (0-255)
- `channel`: Channel identifier (0-255)
- `timeStampNs`: Event timestamp in nanoseconds
- `energy`: Primary energy measurement
- `energyShort`: Short gate energy measurement
- `flags`: Event flags (64-bit input truncated to 32-bit)

## Flag Constants

```cpp
static constexpr uint32_t FLAG_PILEUP = 0x01;
static constexpr uint32_t FLAG_OVER_RANGE = 0x02;
static constexpr uint32_t FLAG_TRIGGER_LOST = 0x04;
static constexpr uint32_t FLAG_ENERGY_SATURATED = 0x08;
```

## Memory Characteristics

- **Size**: 24 bytes (22 bytes of data + 2 bytes padding)
- **Alignment**: 8-byte aligned
- **POD Type**: Yes (Plain Old Data)
- **Trivially Copyable**: Yes

## Transport Integration

### Serialization
MinimalEventData uses FORMAT_VERSION_MINIMAL_EVENTDATA (value: 2) for serialization.

```cpp
// Serialize
Serializer serializer(FORMAT_VERSION_MINIMAL_EVENTDATA);
auto encoded = serializer.Encode(events);

// Deserialize
auto [decoded_events, sequence] = serializer.DecodeMinimalEventData(data);
```

### ZMQTransport Methods
```cpp
// Send MinimalEventData
bool SendMinimal(const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>& events);

// Receive MinimalEventData
std::pair<std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>, uint64_t> ReceiveMinimal();

// Check data type before receiving
DataType PeekDataType();

// Receive with automatic format detection
std::pair<std::variant<EventDataVector, MinimalEventDataVector>, uint64_t> ReceiveAny();
```

## Usage Examples

### Basic Event Creation
```cpp
// Create a single event
MinimalEventData event(1, 5, 12345.67, 1024, 512, 0x01);

// Create via smart pointer
auto event_ptr = std::make_unique<MinimalEventData>(1, 5, 12345.67, 1024, 512, 0x01);
```

### Batch Processing
```cpp
// Create multiple events
auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
for (int i = 0; i < 1000; ++i) {
    events->push_back(std::make_unique<MinimalEventData>(
        i % 16,           // module
        i % 64,           // channel
        i * 1000.0,       // timestamp
        i * 10,           // energy
        i * 5,            // energyShort
        0                 // flags
    ));
}
```

### Network Transport
```cpp
// Setup transport
ZMQTransport transport;
TransportConfig config;
config.data_address = "tcp://localhost:5555";
config.data_pattern = "PUB";
config.bind_data = true;
transport.Configure(config);
transport.Connect();

// Send data
transport.SendMinimal(events);

// Receive with format detection
auto data_type = transport.PeekDataType();
if (data_type == DataType::MINIMAL_EVENTDATA) {
    auto [received_events, seq_num] = transport.ReceiveMinimal();
    // Process received events
}
```

### Format Detection
```cpp
// Automatic format handling
auto [data, seq] = transport.ReceiveAny();
std::visit([](auto&& events) {
    using T = std::decay_t<decltype(events)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>>) {
        // Handle MinimalEventData
        std::cout << "Received MinimalEventData\n";
    } else {
        // Handle EventData
        std::cout << "Received EventData\n";
    }
}, data);
```

## Performance Considerations

### Memory Efficiency
- 96% smaller than EventData (22 bytes vs 500+ bytes)
- Cache-friendly size (fits multiple events per cache line)
- Reduced memory bandwidth requirements

### Processing Speed
- Encoding: 128+ million events/second
- Decoding: 73+ million events/second
- Optimal for high-frequency data acquisition

### Best Practices
1. Use MinimalEventData for high-rate, low-complexity events
2. Batch events for network transport (100-1000 events per batch)
3. Enable compression for long-distance network transport
4. Use memory pools for frequent allocation/deallocation

## Migration from EventData

### When to Use MinimalEventData
- Event rate > 1 MHz
- No waveform data required
- Memory bandwidth is limiting factor
- Network throughput is critical

### Conversion Example
```cpp
// Convert EventData to MinimalEventData
MinimalEventData ConvertToMinimal(const EventData& full_event) {
    return MinimalEventData(
        full_event.module,
        full_event.channel,
        full_event.timeStampNs,
        full_event.energy,
        full_event.energyShort,
        full_event.flags
    );
}
```

## Error Handling

### Common Issues
1. **Format Mismatch**: Use PeekDataType() before receiving
2. **Null Events**: Check for nullptr before sending
3. **Empty Batches**: Validate event count > 0

### Example Error Handling
```cpp
// Safe sending
if (events && !events->empty()) {
    if (!transport.SendMinimal(events)) {
        std::cerr << "Failed to send MinimalEventData\n";
    }
}

// Safe receiving
auto [events, seq] = transport.ReceiveMinimal();
if (events) {
    std::cout << "Received " << events->size() << " events\n";
} else {
    std::cout << "No events available or error occurred\n";
}
```

## See Also
- [EventData API](EventData_API.md) - Full-featured event structure
- [Serializer API](Serializer_API.md) - Serialization framework
- [ZMQTransport API](ZMQTransport_API.md) - Network transport layer
- [MinimalEventData Feature Summary](../MinimalEventData_Feature_Summary.md) - Complete feature overview