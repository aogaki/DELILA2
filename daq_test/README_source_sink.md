# DELILA2 Network Source/Sink Example

This example demonstrates how to separate data acquisition and processing using the DELILA2 network library with ZeroMQ transport.

## Components

### delila_source
Reads data from the digitizer and publishes it over the network.

### delila_sink
Subscribes to the data stream and processes it (monitoring and/or recording).

## Building

```bash
cd build
cmake ..
make delila_source delila_sink
```

## Running

### Terminal 1 - Start the source (publisher)
```bash
./bin/delila_source [config_file] [bind_address] [--compress] [--checksum]

# Example:
./bin/delila_source PSD2.conf tcp://*:5555
```

### Terminal 2 - Start the sink (subscriber)
```bash
./bin/delila_sink [options]

# Options:
#   --address <addr>       Connect address (default: tcp://localhost:5555)
#   --no-monitor          Disable monitoring/visualization
#   --no-recorder         Disable data recording
#   --compress            Enable compression (must match source)
#   --checksum            Enable checksum verification (must match source)
#   --output-prefix <pre> Output file prefix (default: test)
#   --output-dir <dir>    Output directory (default: ./)

# Example:
./bin/delila_sink --address tcp://localhost:5555
```

## Features

- **Decoupled Architecture**: Source and sink can run on different machines
- **Multiple Sinks**: Multiple sink instances can connect to one source
- **Optional Compression**: Enable with --compress flag (both source and sink)
- **Optional Checksums**: Enable with --checksum flag for data integrity
- **Memory Pool**: Optimized memory management for high performance
- **Web Monitoring**: Sink provides web interface for real-time monitoring
- **Data Recording**: Automatic file rotation based on size/event count

## Network Patterns

- Uses ZeroMQ PUB/SUB pattern for data distribution
- Publisher (source) binds to address
- Subscribers (sinks) connect to publisher
- Automatic reconnection on network failures

## Performance Tips

1. Run on same machine: Use `tcp://localhost:5555` for lowest latency
2. Enable compression for network bandwidth limited scenarios
3. Disable checksums for maximum throughput (if data integrity is verified elsewhere)
4. Adjust memory pool size in code for different event rates

## Monitoring

When sink is running with monitor enabled:
- Web interface available at http://localhost:8080
- Real-time histograms and event rate display
- ADC channel histograms for all 32 channels