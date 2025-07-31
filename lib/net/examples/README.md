# DELILA2 Network Library Examples

This directory contains comprehensive examples demonstrating various use cases of the DELILA2 Network Library.

## Building Examples

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Available Examples

### 1. Basic Publisher/Subscriber
- `pubsub_publisher` - Publishes event data continuously
- `pubsub_subscriber` - Subscribes to and displays event data

### 2. Push/Pull Load Balancing
- `push_pull_pusher` - Distributes work to multiple workers
- `push_pull_puller` - Worker that processes distributed tasks

### 3. Command/Control
- `command_server` - REP server handling commands
- `command_client` - REQ client sending commands

### 4. Status Monitoring
- `status_reporter` - Publishes component status
- `status_monitor` - Displays system-wide status

### 5. Configuration
- `config_example` - Demonstrates JSON configuration options

### 6. Performance Testing
- `throughput_test` - Measures data throughput
- `memory_pool_example` - Compares performance with/without memory pools

### 7. Error Handling
- `robust_client` - Demonstrates reconnection and error recovery

## Running Examples

### Basic Pub/Sub
```bash
# Terminal 1
./pubsub_publisher

# Terminal 2
./pubsub_subscriber
```

### Load Balancing
```bash
# Terminal 1
./push_pull_pusher

# Terminal 2, 3, 4
./push_pull_puller 1
./push_pull_puller 2
./push_pull_puller 3
```

### Command/Control
```bash
# Terminal 1
./command_server

# Terminal 2
./command_client
```

### Status Monitoring
```bash
# Terminal 1
./status_reporter

# Terminal 2
./status_monitor
```

## Example Configuration File

See `example_config.json` for a sample JSON configuration that can be used with `config_example` or any transport configuration.

## Notes

- All examples use default ports (5555-5559). Adjust if these ports are in use.
- Examples demonstrate both binding and connecting sides of ZeroMQ patterns.
- Performance examples show optimization techniques for high-throughput scenarios.

## Important Configuration Requirements

The examples have been updated to properly configure socket addresses to avoid conflicts:

1. **Data-only examples** (pubsub, push_pull, throughput_test, etc.) set status and command addresses equal to data address to disable separate channels.

2. **Command server/client** uses different addresses for data/status channels to avoid binding conflicts.

3. **Status reporter/monitor** requires a REQ/REP pattern. The current implementation expects a status server to acknowledge each status message. For simple status broadcasting, consider using PUB/SUB on the data channel instead.

If you encounter "Failed to connect" errors, check that:
- The socket addresses are properly configured
- No address conflicts exist between data, status, and command channels
- For REQ/REP patterns, both server and client sides are running