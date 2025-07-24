# DELILA Network Performance Test

Performance comparison between gRPC and ZeroMQ for high-throughput data acquisition systems.

## Quick Start

1. **Check dependencies:**
   ```bash
   ./scripts/check_deps.sh
   ```

2. **Build the project:**
   ```bash
   ./build.sh
   ```

3. **Run the test:**
   ```bash
   ./scripts/run_test.sh config/config.json grpc
   ```

## Overview

This project implements a comprehensive performance test comparing gRPC and ZeroMQ protocols for high-throughput data acquisition. The test simulates a realistic DAQ scenario with multiple data sources, a central merger, and multiple data sinks.

## Architecture

```
Data Source 1 ──┐
                ├─→ Data Merger ──┬─→ Data Sink 1
Data Source 2 ──┘                └─→ Data Sink 2
```

- **Data Sources** (2): Generate EventData batches and send to merger
- **Data Merger** (1): Receives streams from sources, forwards to sinks
- **Data Sinks** (2): Receive merged EventData stream and generate reports

## Components

### Executables

- **data_sender**: Data source component that generates and sends EventData batches
- **data_hub**: Central merger that receives from sources and forwards to sinks
- **data_receiver**: Data sink that receives batches and generates performance reports

### Usage

```bash
# Data sender
./build/data_sender <config_file> <source_id>

# Data hub
./build/data_hub <config_file>

# Data receiver
./build/data_receiver <config_file> <sink_id>
```

## Configuration

Edit `config/config.json` to customize test parameters:

```json
{
    "test": {
        "protocol": "grpc",           // "grpc" or "zeromq"
        "transport": "tcp",           // "tcp" or "inproc"
        "duration_minutes": 10,       // Test duration
        "batch_sizes": [1024, 10240], // Event batch sizes to test
        "output_dir": "./results"     // Results directory
    },
    "network": {
        "merger_address": "localhost:3389",
        "source1_port": 3390,
        "source2_port": 3391,
        "sink1_port": 3392,
        "sink2_port": 3393
    }
}
```

## Test Scenarios

The test automatically runs through different batch sizes:
- **Small**: 1,024 events (~100KB)
- **Medium**: 10,240 - 512,000 events (1MB - 50MB)
- **Large**: 1,048,576 - 10,485,760 events (100MB - 1GB)

## Performance Metrics

- **Throughput**: Messages per second and MB/s
- **Latency**: Round-trip time percentiles (50th, 90th, 99th)
- **System Resources**: CPU and memory usage
- **Data Integrity**: Sequence validation and gap detection

## Output

### Results Directory
- JSON reports with detailed performance metrics
- Separate files for each test scenario and component

### Logs Directory
- Per-component log files
- Detailed execution traces and error messages

## Dependencies

### Required
- CMake 3.16+
- C++17 compiler (GCC 7+, Clang 5+)
- Protocol Buffers
- gRPC
- ZeroMQ (libzmq)
- cppzmq (header-only)

### Installation (Ubuntu/Debian)
```bash
sudo apt-get install -y \\
    cmake \\
    build-essential \\
    pkg-config \\
    protobuf-compiler \\
    libprotobuf-dev \\
    libgrpc++-dev \\
    libzmq3-dev

# Install cppzmq
git clone https://github.com/zeromq/cppzmq.git
cd cppzmq && mkdir build && cd build
cmake .. && sudo make install
```

## Building

```bash
# Check dependencies
./scripts/check_deps.sh

# Build
./build.sh

# The build creates:
# - build/data_sender
# - build/data_hub
# - build/data_receiver
```

## Running Tests

### Manual Test
```bash
# Start components in separate terminals
./build/data_hub config/config.json
./build/data_receiver config/config.json 1
./build/data_receiver config/config.json 2
./build/data_sender config/config.json 1
./build/data_sender config/config.json 2
```

### Automated Test
```bash
# Run gRPC test
./scripts/run_test.sh config/config.json grpc

# Run ZeroMQ test
./scripts/run_test.sh config/config.json zeromq
```

### Stop All Processes
```bash
./scripts/kill_all.sh
```

## Error Handling

The test includes comprehensive error handling:
- **Memory monitoring**: Stops at 80% RAM usage
- **Network failures**: Immediate test termination
- **Sequence validation**: Detects gaps and duplicates
- **Graceful shutdown**: Signal handling for clean exit

## Customization

### Event Generation
Modify `EventGenerator` class to customize:
- Energy ranges
- Module/channel assignments
- Timing characteristics

### Transport Protocols
Extend `ITransport` interface to add new protocols:
- Implement transport-specific logic
- Add to `TransportFactory`

### Reporting
Customize `ReportGenerator` for different output formats:
- HTML reports with graphs
- CSV data export
- Custom visualizations

## Troubleshooting

### Build Issues
```bash
# Check dependencies
./scripts/check_deps.sh

# Clean build
rm -rf build && ./build.sh
```

### Runtime Issues
```bash
# Check logs
tail -f logs/data_sender.log
tail -f logs/data_hub.log
tail -f logs/data_receiver.log

# Monitor system resources
top -p $(pgrep -f data_)
```

### Network Issues
```bash
# Check port usage
netstat -tlnp | grep :3389

# Test connectivity
telnet localhost 3389
```

## Performance Tips

1. **System Configuration**
   - Increase file descriptor limits
   - Optimize network buffer sizes
   - Use dedicated CPU cores

2. **Test Parameters**
   - Start with small batch sizes
   - Monitor memory usage
   - Adjust test duration based on needs

3. **Hardware Considerations**
   - Use fast storage for logs/results
   - Ensure sufficient RAM for large batches
   - Consider NUMA topology for multi-core systems

## License

This project is part of the DELILA DAQ system and follows the same licensing terms.