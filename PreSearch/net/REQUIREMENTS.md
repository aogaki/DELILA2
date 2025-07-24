# Speed Test: gRPC vs ZeroMQ

## Overview
Performance comparison between gRPC and ZeroMQ for high-throughput data acquisition systems.

## Test Requirements

### Performance Metrics
- **Throughput**: Messages per second
- **Latency**: Round-trip time (RTT) - source sends, sink replies, measure total time
  - Report: mean, min, max, 50th (median), 90th, 99th percentiles
- **CPU Usage**: Resource consumption
- **Memory Usage**: Memory footprint
- **Network Bandwidth**: Data transfer efficiency

### Test Data Structure
- **Primary Data**: `DELILA::Digitizer::EventData` from `lib/digitizer/include/EventData.hpp`
- **Waveform Size**: 0 (no waveform data - only basic event information)
- **Serialization**: Protocol Buffers (protobuf) for both gRPC and ZeroMQ - ensures fair comparison
- **Data Size**: ~100 bytes per EventData (timing, energy, flags, module/channel info)

### Event Batch Sizes
- **Minimum Batch**: 1,024 EventData objects (~100KB total)
- **Maximum Batch**: 10,485,760 EventData objects (10 * 1024 * 1024) (~1GB total)

### Test Scenarios
1. **Small Batches**: 1,024 events (~100KB total)
2. **Medium Batches**:
   - 10 × 1024 = 10,240 events (~1MB)
   - 20 × 1024 = 20,480 events (~2MB)
   - 50 × 1024 = 51,200 events (~5MB)
   - 100 × 1024 = 102,400 events (~10MB)
   - 200 × 1024 = 204,800 events (~20MB)
   - 500 × 1024 = 512,000 events (~50MB)
3. **Large Batches**:
   - 1 × 1024² = 1,048,576 events (~100MB)
   - 2 × 1024² = 2,097,152 events (~200MB)
   - 5 × 1024² = 5,242,880 events (~500MB)
   - 10 × 1024² = 10,485,760 events (~1GB)

### Network Topology
```
Data Source 1 ──┐
                ├─→ Merger ──┬─→ Data Sink 1
Data Source 2 ──┘           └─→ Data Sink 2
```

- **Data Sources** (2): Generate EventData batches and send to merger
- **Merger** (1): Receives streams from both sources, forwards to both sinks
  - Acts as hub/proxy (merging logic not critical)
  - Handles multiple input streams and multiple output streams
- **Data Sinks** (2): Receive merged EventData stream

### Test Environment
- **Hardware**: Single computer (this computer) - no network hardware bottlenecks
- **Network Transport**: TCP and/or InProc (inter-process communication)
- **Port**: 3389 (configurable via configuration file)
- **Focus**: Protocol efficiency comparison without hardware network limitations

### Test Execution
- **Duration**: 10 minutes per test scenario
- **Execution**: Sequential testing (one scenario at a time)
- **Automation**: Automatically test all batch sizes in sequence
- **Method**: Send specific number of events, measure throughput
- **Monitoring**: Real-time status display (throughput, data size, remaining time)
- **Message Boundaries**: Each batch sent as single message (no splitting)
- **Timing**: No warm-up needed, latency not critical for DAQ use case

### Error Handling
- **Network Failures**: Stop test immediately and write failure report
- **High Memory Usage**: Stop test at 80% system RAM usage and document in report
- **Resource Monitoring**: Continuous CPU/memory monitoring
- **Connection Loss**: No reconnection attempts - fail fast to identify issues
- **Data Validation**: Check sequence numbers to verify data integrity
- **Cleanup**: Clean up resources on every run, best effort on interruption

### Implementation Requirements
- **Language**: C++ (primary implementation language)
- **Build System**: CMake
- **Protocols**: Both gRPC and ZeroMQ implementations in C++
- **Data Structures**: Use existing `DELILA::Digitizer::EventData` class
- **Serialization**: Compare serialization efficiency between protocols
- **Threading**: Multi-threaded implementation
  - Merger: 4 worker threads
  - Sources and sinks: as needed
- **Logging**: Separate log file per component
- **Configuration**: JSON configuration file with:
  - Network addresses and ports
  - Batch sizes to test
  - Test duration
  - Protocol selection (gRPC/ZeroMQ)
  - Transport type (TCP/InProc)
  - Output file paths
- **Real-time Display**: Show test status, throughput, data size, remaining time

### Build Targets
- **data_sender**: Data source component
- **data_receiver**: Data sink component  
- **data_hub**: Merger component (if needed)

### Dependencies
- **gRPC**: protobuf, grpc++ (check availability)
  - Client streaming: sources to merger
  - Server streaming: merger to sinks
- **ZeroMQ**: libzmq, cppzmq (check availability)
  - PUSH/PULL pattern: sources to merger
  - PUB/SUB pattern: merger to sinks
- **CMake**: Build system
- **System Monitoring**: CPU/memory usage tracking

### Output Format
- **Report Format**: HTML report with graphs and visualizations
- **Comparative analysis**: Direct comparison between gRPC and ZeroMQ
- **Error reporting**: Network failures and resource issues
- **Performance metrics**: Throughput, latency, resource usage
- **Visualizations**: 
  - Throughput vs batch size graphs
  - Latency distribution charts
  - CPU/Memory usage over time
  - Protocol comparison charts

### Deployment
- **Environment**: Single computer (no containers)
- **Process Management**: Multiple processes for topology simulation
- **Configuration**: File-based address/port configuration