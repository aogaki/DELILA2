# ZMQTransport Integration Test Guide

## Overview

This guide documents the integration tests for the ZMQTransport Phase 1 REQ/REP implementation and explains the fixes that were applied to resolve test failures.

## Key Issues and Resolutions

### 1. Socket Address Conflicts

**Problem**: Multiple ZMQTransport instances were experiencing "Address already in use" errors when trying to bind to the same default addresses.

**Root Cause**: The TransportConfig struct has default addresses:
- `status_address = "tcp://localhost:5556"`
- `command_address = "tcp://localhost:5557"`

When multiple transports are created in the same test, they may attempt to bind to these default addresses even if not explicitly using those channels.

**Solution**: Each transport instance must have unique addresses for ALL channels:
```cpp
// Server configuration
server_config.data_address = "tcp://127.0.0.1:25555";
server_config.command_address = "tcp://127.0.0.1:25557";
server_config.status_address = "tcp://127.0.0.1:25556";  // Unique!

// Client configuration  
client_config.data_address = "tcp://127.0.0.1:25554";
client_config.command_address = "tcp://127.0.0.1:25557";  // Same as server (client connects)
client_config.status_address = "tcp://127.0.0.1:25558";  // Unique!
```

### 2. Metrics Serialization

**Problem**: The `HandlesStatusRequests` test was failing because metrics data was not being serialized/deserialized.

**Root Cause**: The `SerializeStatus` and `DeserializeStatus` methods were not handling the `std::map<std::string, double> metrics` field in ComponentStatus.

**Solution**: Added JSON serialization for metrics map:
```cpp
// In SerializeStatus
if (!status.metrics.empty()) {
  json << ",\"metrics\":{";
  bool first = true;
  for (const auto& [key, value] : status.metrics) {
    if (!first) json << ",";
    json << "\"" << key << "\":" << value;
    first = false;
  }
  json << "}";
}

// In DeserializeStatus - parse metrics from JSON
```

## Integration Test Structure

### Test 1: HandlesIncomingCommands
- **Purpose**: Verify REQ/REP pattern for command handling
- **Setup**: Server with REP socket, client with REQ socket
- **Flow**: Client sends command → Server receives → Server sends response → Client receives

### Test 2: HandlesStatusRequests  
- **Purpose**: Verify REQ/REP pattern for status requests with metrics
- **Setup**: Server with status REP socket, client with status REQ socket
- **Flow**: Client triggers request → Server sends status with metrics → Client validates

### Test 3: BidirectionalCommunication
- **Purpose**: Verify a transport can act as both client and server
- **Setup**: Transport A (PUB data + REP commands), Transport B (SUB data + REQ commands)
- **Flow**: A sends data to B via PUB/SUB, B sends command to A via REQ/REP

## Best Practices for ZMQTransport Tests

1. **Always set unique addresses** for each transport instance in tests
2. **Explicit configuration** - Don't rely on defaults when multiple transports exist
3. **Proper cleanup** - Ensure transports are properly disconnected in TearDown
4. **Timing considerations** - Add small delays after Connect() for socket establishment
5. **Thread synchronization** - Use atomic flags and proper thread joins

## Common Pitfalls

1. **Default address conflicts** - Even unused channels may attempt to bind
2. **Socket type mismatches** - Ensure REQ connects to REP, PUB to SUB, etc.
3. **Timing issues** - ZeroMQ needs time to establish connections
4. **Missing data fields** - Ensure all ComponentStatus fields are serialized

## Running the Tests

```bash
# Build the tests
make test_zmq_transport_phase1_integration

# Run all integration tests
./lib/net/test_zmq_transport_phase1_integration

# Run specific test
./lib/net/test_zmq_transport_phase1_integration --gtest_filter=ZMQTransportIntegrationTest.HandlesIncomingCommands
```

## Debugging Failed Tests

1. Check for "Address already in use" errors in output
2. Verify all transports have unique addresses
3. Add debug output to Connect() method temporarily
4. Use `netstat -an | grep <port>` to check if ports are in use
5. Ensure proper socket cleanup between tests

## Future Improvements

- Consider dynamic port allocation for tests
- Add more comprehensive error messages in Connect() failures
- Implement connection retry logic for transient failures
- Add performance benchmarks for REQ/REP patterns