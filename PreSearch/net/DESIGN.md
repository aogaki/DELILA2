# Software Design: gRPC vs ZeroMQ Speed Test

## Overview
This document describes the software design for the performance comparison test between gRPC and ZeroMQ protocols for high-throughput data acquisition systems based on REQUIREMENTS.md.

## Architecture

### System Components

```
┌─────────────┐    ┌─────────────┐
│Data Source 1│    │Data Source 2│
│(data_sender)│    │(data_sender)│
└──────┬──────┘    └──────┬──────┘
       │                  │
       │ EventData        │ EventData
       │ batches          │ batches
       │                  │
    ┌──▼──────────────────▼──┐
    │      Data Merger       │
    │      (data_hub)        │
    │    [4 threads]         │
    └──┬──────────────────┬──┘
       │                  │
       │ Merged           │ Merged
       │ EventData        │ EventData
       │                  │
┌──────▼──────┐    ┌──────▼──────┐
│ Data Sink 1 │    │ Data Sink 2 │
│(data_receiver)    │(data_receiver)
└─────────────┘    └─────────────┘
```

### Component Responsibilities

1. **Data Source (data_sender)**
   - Generate EventData batches
   - Send data to merger using configured protocol
   - Track sequence numbers
   - Monitor sending rate

2. **Data Merger (data_hub)**
   - Receive data from multiple sources
   - Forward to multiple sinks
   - Maintain 4 worker threads
   - No data transformation (pure forwarding)

3. **Data Sink (data_receiver)**
   - Receive EventData from merger
   - Validate sequence numbers
   - Calculate statistics
   - Generate performance reports

## Class Design

### Core Classes

```cpp
// Base transport interface
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool Initialize(const Config& config) = 0;
    virtual bool Send(const EventDataBatch& batch) = 0;
    virtual bool Receive(EventDataBatch& batch) = 0;
    virtual void Shutdown() = 0;
    virtual std::string GetStats() const = 0;
};

// Protocol implementations
class GrpcTransport : public ITransport {
    // Client streaming for sending
    // Server streaming for receiving
};

class ZmqTransport : public ITransport {
    // PUSH/PULL for source->merger
    // PUB/SUB for merger->sink
};

// Event data batch container
class EventDataBatch {
public:
    std::vector<DELILA::Digitizer::EventData> events;
    uint64_t sequenceNumber;
    uint64_t timestamp;
    uint32_t sourceId;
    
    // Serialization methods
    bool SerializeToProtobuf(EventBatchProto& proto) const;
    bool DeserializeFromProtobuf(const EventBatchProto& proto);
};

// Statistics collector
class StatsCollector {
    std::atomic<uint64_t> messagesReceived;
    std::atomic<uint64_t> bytesReceived;
    std::vector<double> latencies;
    std::chrono::steady_clock::time_point startTime;
    
public:
    void RecordMessage(size_t bytes, double latency);
    StatsReport GenerateReport() const;
};
```

### Application Classes

```cpp
class DataSource {
    std::unique_ptr<ITransport> transport;
    EventDataGenerator generator;
    StatsCollector stats;
    uint32_t sourceId;
    
public:
    void Run(const TestScenario& scenario);
};

class DataMerger {
    std::vector<std::unique_ptr<ITransport>> receivers;
    std::vector<std::unique_ptr<ITransport>> senders;
    ThreadPool workerPool{4};
    
public:
    void Run();
};

class DataSink {
    std::unique_ptr<ITransport> transport;
    SequenceValidator validator;
    StatsCollector stats;
    
public:
    void Run();
    void GenerateReport(const std::string& outputPath);
};
```

## Protocol Design

### Protobuf Message Definitions

```protobuf
syntax = "proto3";

package delila.test;

// EventData protobuf mapping
message EventDataProto {
    double timestamp_ns = 1;
    uint32 energy = 2;
    uint32 energy_short = 3;
    uint32 module = 4;
    uint32 channel = 5;
    uint32 time_resolution = 6;
    uint64 flags = 7;
    // No waveform data (size = 0)
}

// Batch of events
message EventBatchProto {
    repeated EventDataProto events = 1;
    uint64 sequence_number = 2;
    uint64 timestamp = 3;
    uint32 source_id = 4;
}

// gRPC service definition
service DataStream {
    // Source to merger
    rpc SendData(stream EventBatchProto) returns (Acknowledgment);
    
    // Merger to sink
    rpc ReceiveData(SubscribeRequest) returns (stream EventBatchProto);
}

message Acknowledgment {
    bool success = 1;
    uint64 last_sequence = 2;
}

message SubscribeRequest {
    string sink_id = 1;
}
```

### ZeroMQ Message Format

```cpp
// ZMQ uses same protobuf serialization
// Message envelope for PUB/SUB
struct ZmqEnvelope {
    char topic[32];  // "DATA" for event data
    uint32_t size;   // Protobuf size
    // Followed by serialized EventBatchProto
};
```

## Configuration Design

### JSON Configuration Structure

```json
{
    "test": {
        "protocol": "grpc",  // or "zeromq"
        "transport": "tcp",   // or "inproc"
        "duration_minutes": 10,
        "batch_sizes": [1024, 10240, 20480, 51200, 102400, 204800, 512000,
                        1048576, 2097152, 5242880, 10485760],
        "output_dir": "./results"
    },
    
    "network": {
        "merger_address": "localhost:3389",
        "source1_port": 3390,
        "source2_port": 3391,
        "sink1_port": 3392,
        "sink2_port": 3393
    },
    
    "grpc": {
        "max_message_size": 2147483647,  // 2GB
        "keepalive_time_ms": 10000
    },
    
    "zeromq": {
        "high_water_mark": 0,  // unlimited
        "linger_ms": 1000,
        "rcv_buffer_size": 4194304  // 4MB
    },
    
    "logging": {
        "level": "info",
        "directory": "./logs"
    }
}
```

## Implementation Details

### Test Execution Flow

```cpp
class TestRunner {
    void RunTest() {
        // 1. Load configuration
        Config config = LoadConfig("config.json");
        
        // 2. For each batch size
        for (auto batchSize : config.batchSizes) {
            // 3. Launch components
            LaunchTopology(config, batchSize);
            
            // 4. Run for duration
            RunForDuration(config.durationMinutes);
            
            // 5. Collect results
            CollectResults();
            
            // 6. Cleanup
            ShutdownTopology();
        }
        
        // 7. Generate HTML report
        GenerateHTMLReport();
    }
};
```

### Memory Management

```cpp
class MemoryMonitor {
    const double MEMORY_THRESHOLD = 0.8;  // 80% RAM
    
    bool CheckMemoryUsage() {
        auto usage = GetSystemMemoryUsage();
        if (usage > MEMORY_THRESHOLD) {
            LogError("Memory usage exceeded 80%");
            return false;
        }
        return true;
    }
};
```

### Real-time Display

```cpp
class ProgressDisplay {
    void UpdateDisplay(const Stats& stats) {
        // Clear screen and show:
        // - Current test: gRPC/ZeroMQ
        // - Batch size: X events
        // - Throughput: Y MB/s
        // - Messages/sec: Z
        // - Time remaining: MM:SS
        // - Memory usage: XX%
        // - CPU usage: YY%
    }
};
```

## Thread Model

### Data Merger Threading

```cpp
class DataMerger {
    void ProcessingLoop() {
        // Main thread: accept connections
        
        // Worker threads (4): process messages
        for (int i = 0; i < 4; ++i) {
            workerPool.submit([this] {
                while (running) {
                    EventBatchProto batch;
                    if (inputQueue.pop(batch)) {
                        // Forward to all sinks
                        for (auto& sink : senders) {
                            sink->Send(batch);
                        }
                    }
                }
            });
        }
    }
};
```

## Error Handling

```cpp
class ErrorHandler {
    void HandleNetworkError(const std::exception& e) {
        // 1. Log error
        LOG_ERROR("Network failure: " << e.what());
        
        // 2. Stop test immediately
        StopTest();
        
        // 3. Write failure report
        WriteFailureReport(e);
        
        // 4. Clean up resources
        CleanupResources();
    }
};
```

## Build System

### CMake Structure

```cmake
cmake_minimum_required(VERSION 3.16)
project(delila_net_test)

# Find dependencies
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(cppzmq REQUIRED)
find_package(Threads REQUIRED)

# Generate protobuf/grpc files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/messages.proto)
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS proto/messages.proto)

# Common library
add_library(test_common
    src/EventDataBatch.cpp
    src/StatsCollector.cpp
    src/ConfigLoader.cpp
    ${PROTO_SRCS}
)

# Transport implementations
add_library(transports
    src/GrpcTransport.cpp
    src/ZmqTransport.cpp
    ${GRPC_SRCS}
)

# Executables
add_executable(data_sender src/DataSource.cpp)
add_executable(data_hub src/DataMerger.cpp)
add_executable(data_receiver src/DataSink.cpp)

# Link libraries
target_link_libraries(data_sender 
    test_common transports 
    ${Protobuf_LIBRARIES} gRPC::grpc++ cppzmq
)
# ... similar for other targets
```

## Testing Strategy

1. **Unit Tests**: Test individual components
2. **Integration Tests**: Test protocol implementations
3. **Performance Tests**: Automated test runs
4. **Validation**: Sequence number checking
5. **Memory Leak Detection**: Valgrind/AddressSanitizer

## Next Steps

1. Implement base classes and interfaces
2. Create protobuf definitions
3. Implement gRPC transport
4. Implement ZeroMQ transport
5. Build test applications
6. Create HTML report generator
7. Run performance tests