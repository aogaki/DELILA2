# DELILA Network Library Environment Variables

This document describes all environment variables that can be used to override configuration settings.

## Profile Selection

| Variable | Description | Example |
|----------|-------------|---------|
| `DELILA_NET_PROFILE` | Active configuration profile | `production`, `development`, `test`, `benchmark` |

## Buffer Settings

| Variable | Description | Default | Valid Range |
|----------|-------------|---------|-------------|
| `DELILA_NET_HIGH_WATER_MARK` | ZeroMQ high water mark (messages) | 2000 | 1-100000 |
| `DELILA_NET_RECEIVE_BUFFER_SIZE` | Internal receive buffer size (messages) | 1000 | 1-50000 |
| `DELILA_NET_BUFFER_POOL_SIZE_GB` | Pre-allocated buffer pool size (GB) | 4 | 0.1-1024 |

## Performance Settings

| Variable | Description | Default | Valid Values |
|----------|-------------|---------|--------------|
| `DELILA_NET_COMPRESSION_ENABLED` | Enable/disable compression | true | true, false |
| `DELILA_NET_COMPRESSION_LEVEL` | Compression level | 1 | 1-12 |
| `DELILA_NET_COMPRESSION_ALGORITHM` | Compression algorithm | lz4 | lz4, zstd, snappy |
| `DELILA_NET_TIMEOUT_MS` | Network operation timeout (ms) | 5000 | 100-300000 |

## Error Handling

| Variable | Description | Default | Valid Range |
|----------|-------------|---------|-------------|
| `DELILA_NET_GAP_ALERT_THRESHOLD` | Sequence gaps before alerting | 10 | 1-1000 |
| `DELILA_NET_GAP_ALERT_WINDOW_MINUTES` | Gap counting time window (minutes) | 1 | 0.1-60 |
| `DELILA_NET_MAX_RETRY_ATTEMPTS` | Maximum retry attempts (-1=infinite) | -1 | -1 or 1+ |
| `DELILA_NET_INITIAL_RETRY_INTERVAL_MS` | Initial retry interval (ms) | 100 | 1-60000 |
| `DELILA_NET_MAX_RETRY_INTERVAL_MS` | Maximum retry interval (ms) | 30000 | 100-300000 |
| `DELILA_NET_RETRY_BACKOFF_MULTIPLIER` | Exponential backoff multiplier | 2.0 | 1.0-10.0 |

## Network Settings

| Variable | Description | Default | Valid Values |
|----------|-------------|---------|--------------|
| `DELILA_NET_DEFAULT_INTERFACE` | Default network interface | eth0 | Interface name or empty |
| `DELILA_NET_DEFAULT_TRANSPORT` | Default transport type | auto | auto, tcp, ipc |
| `DELILA_NET_TCP_NO_DELAY` | Disable Nagle's algorithm | true | true, false |
| `DELILA_NET_TCP_KEEP_ALIVE` | Enable TCP keep-alive | true | true, false |
| `DELILA_NET_TCP_LINGER_MS` | Socket linger time (ms) | 1000 | 0-60000 |
| `DELILA_NET_IPC_TEMP_DIRECTORY` | IPC socket directory | /tmp/delila_ipc | Directory path |

## Logging Settings

| Variable | Description | Default | Valid Values |
|----------|-------------|---------|--------------|
| `DELILA_NET_LOG_LEVEL` | Logging level | info | trace, debug, info, warn, error, fatal |
| `DELILA_NET_LOG_CONNECTION_EVENTS` | Log connection events | true | true, false |
| `DELILA_NET_LOG_PERFORMANCE_METRICS` | Log performance metrics | true | true, false |
| `DELILA_NET_LOG_SEQUENCE_GAPS` | Log sequence gaps | true | true, false |

## Endpoint Overrides

| Variable | Description | Default | Format |
|----------|-------------|---------|--------|
| `DELILA_NET_DATA_PUBLISHER_ENDPOINT` | Data publisher endpoint | tcp://*:5555 | tcp://host:port or ipc://path |
| `DELILA_NET_CONTROL_SERVER_ENDPOINT` | Control server endpoint | tcp://*:5556 | tcp://host:port or ipc://path |
| `DELILA_NET_STATUS_COLLECTOR_ENDPOINT` | Status collector endpoint | tcp://*:5557 | tcp://host:port or ipc://path |

## Usage Examples

### Development Environment
```bash
export DELILA_NET_PROFILE=development
export DELILA_NET_LOG_LEVEL=debug
export DELILA_NET_COMPRESSION_ENABLED=false
export DELILA_NET_DEFAULT_TRANSPORT=ipc
```

### Production Environment
```bash
export DELILA_NET_PROFILE=production
export DELILA_NET_DEFAULT_INTERFACE=eth0
export DELILA_NET_HIGH_WATER_MARK=4000
export DELILA_NET_BUFFER_POOL_SIZE_GB=8
export DELILA_NET_LOG_LEVEL=info
```

### High-Performance Testing
```bash
export DELILA_NET_PROFILE=benchmark
export DELILA_NET_HIGH_WATER_MARK=10000
export DELILA_NET_BUFFER_POOL_SIZE_GB=16
export DELILA_NET_COMPRESSION_LEVEL=3
export DELILA_NET_LOG_LEVEL=warn
export DELILA_NET_LOG_PERFORMANCE_METRICS=true
```

### Multi-Machine Setup
```bash
export DELILA_NET_DEFAULT_TRANSPORT=tcp
export DELILA_NET_DATA_PUBLISHER_ENDPOINT="tcp://192.168.1.100:5555"
export DELILA_NET_CONTROL_SERVER_ENDPOINT="tcp://192.168.1.100:5556"
export DELILA_NET_STATUS_COLLECTOR_ENDPOINT="tcp://192.168.1.100:5557"
```

### Testing Configuration
```bash
export DELILA_NET_PROFILE=test
export DELILA_NET_DEFAULT_TRANSPORT=ipc
export DELILA_NET_IPC_TEMP_DIRECTORY="/tmp/delila_test"
export DELILA_NET_TIMEOUT_MS=1000
export DELILA_NET_MAX_RETRY_ATTEMPTS=3
```

## Configuration Priority

Settings are applied in the following order (highest priority first):

1. **Environment variables** (highest priority)
2. **Command line arguments** (if supported by application)
3. **Configuration file active profile**
4. **Configuration file default values**
5. **Compiled-in defaults** (lowest priority)

## Validation

All environment variables are validated according to the same rules as configuration files:
- Numeric ranges are enforced
- String values must match allowed enums
- File paths are checked for existence where applicable
- Network interfaces are validated if specified

Invalid environment variables will cause the library to log warnings and fall back to configuration file or default values.