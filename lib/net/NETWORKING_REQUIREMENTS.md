# DELILA2 Network Library - ZeroMQ Integration Requirements

## Overview

This document specifies the requirements for implementing a ZeroMQ-based networking layer in the DELILA2 data acquisition system. The network library will provide high-performance, reliable communication between DAQ components using ZeroMQ as the underlying transport mechanism.

## Architecture Goals

- **High Performance**: Minimize latency and maximize throughput for physics data
- **Reliability**: Ensure data integrity and delivery guarantees where needed
- **Flexibility**: Support multiple communication patterns and topologies
- **Integration**: Seamless integration with existing Serializer and EventData components
- **Scalability**: Support distributed DAQ systems with multiple components

## Core Components

### 1. ZMQTransport Class

A wrapper class that provides a simplified interface to ZeroMQ functionality while maintaining flexibility for different communication patterns.

#### Key Features:
- **Pattern Support**: REQ/REP, PUB/SUB, PUSH/PULL, PAIR
- **Transport Protocols**: TCP, IPC, InProc
- **Connection Management**: Automatic reconnection and error handling
- **Configuration**: Runtime configuration of addresses, ports, and patterns

### 2. Communication Channels

The ZMQTransport class will support three distinct communication channels:

#### 2.1 Data Channel (EventData Transport)
- **Purpose**: High-throughput transport of physics event data
- **Protocol**: Binary format using existing Serializer class
- **Pattern**: Typically PUB/SUB or PUSH/PULL for streaming data
- **Features**: 
  - Compression support via Serializer
  - Checksums for data integrity
  - Sequence numbering for gap detection

#### 2.2 Status Channel (Component Health Monitoring)
- **Purpose**: Monitor and report DAQ component status
- **Protocol**: JSON or lightweight binary format
- **Pattern**: PUB/SUB for status broadcasts
- **Features**:
  - Component health indicators
  - Performance metrics
  - Error reporting
  - Heartbeat mechanism

#### 2.3 Command Channel (State Machine Control)
- **Purpose**: Send commands and control DAQ component state machines
- **Protocol**: JSON or structured text format
- **Pattern**: REQ/REP for command acknowledgment
- **Features**:
  - State transition commands
  - Configuration updates
  - Synchronous command execution
  - Response validation

## Functional Requirements

### FR-1: Data Transport Functions

```cpp
// Send serialized EventData
bool Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events, 
          uint64_t sequenceNumber = 0);

// Receive and deserialize EventData
std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> 
    Receive(int timeout_ms = -1);
```

### FR-2: Status Management Functions

```cpp
// Send component status information
bool SendStatus(const ComponentStatus& status);

// Receive status from remote components
std::unique_ptr<ComponentStatus> ReceiveStatus(int timeout_ms = -1);
```

### FR-3: Command Management Functions

```cpp
// Send command to remote component
CommandResponse SendCommand(const Command& command, int timeout_ms = 5000);

// Receive command (for component implementing command handler)
std::unique_ptr<Command> ReceiveCommand(int timeout_ms = -1);

// Send command response
bool SendCommandResponse(const CommandResponse& response);
```

### FR-4: Configuration and Connection Management

```cpp
// Configure transport parameters
bool Configure(const TransportConfig& config);

// Establish connections
bool Connect();

// Close connections gracefully
void Disconnect();

// Check connection status
bool IsConnected() const;
```

## Data Structures

### ComponentStatus
```cpp
struct ComponentStatus {
    std::string component_id;
    std::string state;              // e.g., "IDLE", "RUNNING", "ERROR"
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, double> metrics;  // Performance metrics
    std::string error_message;      // Empty if no error
    uint64_t heartbeat_counter;
};
```

### Command and CommandResponse
```cpp
enum class CommandType {
    START,
    STOP,
    PAUSE,
    RESUME,
    CONFIGURE,
    RESET,
    SHUTDOWN
};

struct Command {
    std::string command_id;         // Unique identifier
    CommandType type;
    std::string target_component;
    std::map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point timestamp;
};

struct CommandResponse {
    std::string command_id;         // Matches Command.command_id
    bool success;
    std::string message;            // Error message or status info
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> result_data;
};
```

### TransportConfig
```cpp
struct TransportConfig {
    // Data channel configuration
    struct {
        std::string pattern;        // "PUB", "SUB", "PUSH", "PULL"
        std::string address;        // e.g., "tcp://localhost:5555"
        std::string topic;          // For PUB/SUB pattern
        bool bind;                  // true to bind, false to connect
    } data_channel;
    
    // Status channel configuration
    struct {
        std::string pattern;        // Typically "PUB" or "SUB"
        std::string address;        // e.g., "tcp://localhost:5556"
        std::string topic;          // Status topic filter
        bool bind;
    } status_channel;
    
    // Command channel configuration
    struct {
        std::string pattern;        // "REQ" or "REP"
        std::string address;        // e.g., "tcp://localhost:5557"
        bool bind;
    } command_channel;
    
    // General settings
    int receive_timeout_ms = 1000;
    int send_timeout_ms = 1000;
    int linger_ms = 1000;
    bool enable_monitoring = true;
};
```

## Non-Functional Requirements

### NFR-1: Performance
- **Data Throughput**: Support >100 MB/s for event data streaming
- **Latency**: Command response time <100ms under normal conditions
- **Memory Usage**: Efficient memory management, minimal copying

### NFR-2: Reliability
- **Data Integrity**: Use existing Serializer checksums for data validation
- **Connection Recovery**: Automatic reconnection with exponential backoff
- **Error Handling**: Comprehensive error reporting and graceful degradation

### NFR-3: Scalability
- **Multiple Clients**: Support many-to-one and one-to-many communication
- **Distributed Systems**: Work across network boundaries
- **Resource Management**: Efficient use of network and system resources

### NFR-4: Maintainability
- **Clear API**: Simple, intuitive interface following C++ best practices
- **Logging**: Comprehensive logging for debugging and monitoring
- **Testing**: Full unit test coverage
- **Documentation**: Clear API documentation and usage examples

## Implementation Considerations

### Dependencies
- **ZeroMQ**: libzmq >= 4.3.0
- **JSON Library**: nlohmann/json for status/command serialization
- **Threading**: std::thread for asynchronous operations
- **Existing**: Integration with current Serializer and EventData classes

### Error Handling Strategy
- Use exceptions for configuration and setup errors
- Return error codes/status for runtime operations
- Provide detailed error messages and context
- Implement timeout mechanisms for all blocking operations

### Thread Safety
- The ZMQTransport class should be thread-safe for simultaneous operations on different channels
- Use appropriate synchronization mechanisms (mutexes, atomic operations)
- Consider separate threads for different communication channels

### Configuration Management
- Support both programmatic configuration and configuration files
- Provide sensible defaults for common use cases
- Validate configuration parameters at setup time

## Testing Requirements

### Unit Tests
- Test each communication channel independently
- Mock ZeroMQ operations for isolated testing
- Test error conditions and edge cases
- Performance benchmarks for throughput and latency

### Integration Tests
- Test communication between actual ZeroMQ sockets
- Test with real Serializer data
- Test network failure scenarios
- Test multi-component communication patterns

### Performance Tests
- Measure data throughput under various conditions
- Measure command/response latency
- Memory usage profiling
- Network bandwidth utilization

## Future Considerations

### Advanced Features (Phase 2)
- **Security**: Encryption and authentication support
- **Discovery**: Automatic service discovery mechanisms
- **Load Balancing**: Intelligent routing for high-availability setups
- **Monitoring**: Integration with monitoring systems (Prometheus, etc.)
- **Compression**: Additional compression algorithms beyond LZ4

### Protocol Extensions
- **Metadata**: Additional metadata fields for routing and filtering
- **Versioning**: Protocol version negotiation
- **Flow Control**: Backpressure mechanisms for high-throughput scenarios

## Success Criteria

1. **Functional**: All specified functions work correctly with comprehensive test coverage
2. **Performance**: Meet or exceed performance requirements under load
3. **Integration**: Seamless integration with existing DELILA2 components
4. **Reliability**: Stable operation under normal and error conditions
5. **Usability**: Clear, documented API that is easy to use correctly

This requirements document serves as the foundation for implementing a robust, high-performance networking layer that meets the needs of the DELILA2 data acquisition system while providing flexibility for future enhancements.