# DELILA2 Networking Library Requirements

## Overview
A high-performance networking library for DELILA DAQ software using ZeroMQ.

## Core Requirements

### 1. Network Protocols
- ZeroMQ with TCP support
- ZeroMQ with Inter-Process Communication (IPC) support
- **Socket Configuration**:
  - High Water Mark (HWM): Configurable send/receive buffer limits
  - Timeouts: 5 second send/receive timeouts for reliability
  - TCP Keepalive: Enabled with 5-minute idle, 30-second intervals
- **Connection String Formats**:
  - TCP: `tcp://ip:port` (e.g., `tcp://192.168.1.100:5555`)
  - IPC: `ipc:///tmp/delila_<name>` (e.g., `ipc:///tmp/delila_data`)
  - Interface binding: Use IP address resolution (`tcp://0.0.0.0:port` for all interfaces)
- **Transport Selection**: Automatic IPC for localhost/127.0.0.1, TCP for remote hosts

### 2. ZeroMQ Messaging Patterns
- **PUB/SUB**: 
  - Data distribution: Broadcasting binary experimental data from merger to sinks
  - Status reporting: Modules publish status/error messages, controller subscribes
- **PUSH/PULL**: 
  - Work distribution: Sending JSON histogram information
  - Error/event collection: Collecting asynchronous notifications from modules
- **REQ/REP**: 
  - Simple synchronous control: Basic heartbeat checks and simple queries
- **DEALER/ROUTER**: 
  - Asynchronous control: Non-blocking controller operations with multiple modules
  - Module coordination: Merger coordinating with multiple data fetchers

### 3. System Architecture
- Flexible topology support (star, pipeline, hybrid)
- Scalable to support varying numbers of modules
- Example use cases:
  - Multiple data fetchers (3-20+)
  - Data processing modules (merger, event builder)
  - Data sink modules (recorder, monitor)
  - Controller module for system coordination

### 4. Data Flow
- Data fetchers → Merger (binary data via PUSH/PULL or DEALER/ROUTER)
- Merger → Data sink modules (sorted by timestamp via PUB/SUB)
- Controller ↔ All modules (control messages via DEALER/ROUTER or REQ/REP)
- All modules → Controller (status/error reports via PUB/SUB or PUSH/PULL)

### 5. Performance Requirements
- **Priority**: Maximum data rate (latency not critical)
- **Target Rate**: 100 MB/s typical
- **Maximum Rate**: ~500 MB/s (limited by HDD write speeds)

### 6. Data Serialization
- **Binary experimental data**: Custom binary format with LZ4 compression
  - Header (uncompressed, fixed 64 bytes): 
    - Magic number: "DELILA2\0" identifier
    - Sequence number: for gap detection (starts at 0)
    - Format version: binary format version (starts at 1)
    - Header size: size of header (always 64 bytes)
    - Event count: number of events in payload
    - Uncompressed size: size before compression
    - Compressed size: size after compression (equals uncompressed_size if no compression)
    - Checksum: xxHash32 of payload data (fastest performance)
    - Timestamp: Unix timestamp in nanoseconds since epoch
    - Reserved fields: for future expansion
  - Body: LZ4-compressed packed EventData structures (from EventData.hpp)
  - All multi-byte integers use little-endian byte order
  - Self-describing with minimal overhead
  - Header remains uncompressed for easy parsing
  - **EventData Serialization Rules**:
    - Fixed-size header: 44 bytes (packed, no padding)
    - Fields serialized in ALPHABETICAL ORDER for consistency
    - waveformSize serialized as uint32_t (not size_t)
    - Waveform vectors serialized in alphabetical order: analogProbe1, analogProbe2, digitalProbe1-4
    - All vectors have same size (waveformSize)
  - **Compression Control**:
    - Configurable compression enable/disable via BinarySerializer methods
    - Compression level settable (1-12, LZ4 levels)
    - Compression applied if: enable_compression=true AND compression_level > 0
    - Empty event vectors return empty payload (success, not error)
    - No maximum events-per-message limit enforced
- **Histogram data**: JSON
- **Control messages**: Protocol Buffers with complete message specifications:
  - Module identification (module_id, instance_id) in all messages
  - Timestamps for all messages  
  - Request/response correlation IDs for tracking
  - State transitions with previous/current state and 5-second timeouts
  - Network status metrics (bytes, rates, errors, gaps, drops)
  - Error categories and suggested recovery actions
  - No priority levels or stack traces

### 7. Error Handling & Recovery
- Automatic reconnection with immediate retry attempts followed by exponential backoff
- Message retry mechanism for transient failures
- Timeout handling with immediate return (non-blocking operations)
- Error callbacks via logging system
- **API Behavior**: Log errors and continue operation when possible, auto-retry on transient failures
- **Operation Mode**: All send/receive operations are non-blocking (return immediately)

### 8. Logging Requirements
- Connection status logging
- Data transfer size tracking
- Data transfer rate monitoring

### 9. Platform Support
- Linux
- macOS

### 10. Programming Language
- C++ implementation
- **C++ Standard**: C++17 minimum requirement
- **Compiler Support**: GCC 7+, Clang 5+, AppleClang 10+
- **Optimization Level**: -O2 for release builds
- **Warning Level**: All warnings enabled and treated as errors

### 11. Security
- No security at network library level
- Security handled at application level (Web UI with Oat++)

### 12. Message Buffering & Flow Control
- ZeroMQ High Water Mark (HWM) configuration for overflow protection
- Module-side buffering with std::deque
- Dedicated receiving thread per module
- Drop messages at ZeroMQ level when buffer full
- Sequence gap detection: log and continue
- Alert generation when gap frequency exceeds threshold

### 13. Network Configuration
- Support binding to specific network interfaces (e.g., eth0, eth1)
- Configurable per connection (data vs control traffic separation)
- IPv4 support (IPv6 not required)
- Flexible port assignment
- **Configuration Structure**: Array-based JSON for multiple network entities per module
- **Configuration Priority**: Environment variables override config file values
- **Validation**: All fields required, port ranges (1-65535), buffer limits enforced
- **Error Handling**: Invalid configuration causes startup failure with detailed error reporting

### 14. Message Priorities
- Experimental data: Highest priority
- Emergency stop commands: High priority
- State change commands: High priority
- Heartbeats: Medium priority
- Status reports: Low priority

### 15. Testing & Debugging
- **Test Data Generation**: Random test data with sine wave waveforms
- **Performance Benchmarks**: 10 MB/s throughput target for test validation
- **Test Organization**: Separate executables for unit/integration/benchmark tests
- **Test Execution**: Sequential test execution (no parallel testing)
- **Mock Objects**: Mock ZeroMQ sockets, network interfaces, and EventData generators
- **Test Data Loading**: Support for loading real experimental data from files
- **Throughput measurement tools and latency capabilities for validation

### 16. Memory Management
- Simple allocation per message (no complex memory pools)
- Memory pooling handled at module level, not in network library
- Direct binary serialization using memcpy for POD fields
- Variable-size message support
- On resource exhaustion: return error immediately (no blocking)
- Use ZeroMQ features for zero-copy where possible

### 17. Thread Safety Requirements
- **ZeroMQ Context**: One context per process, shared by all threads
- **Connection Objects**: Thread-local only (no sharing between threads)
- **ZeroMQ Sockets**: Never share sockets between threads
- **Receive Buffers**: Single producer/single consumer lock-free queue
- **Serializers**: Thread-local instances (no sharing)
- **Statistics**: Lock-free atomic counters (std::atomic)
- **General Rule**: Each thread owns its resources, no sharing of stateful objects

### 18. Platform-Specific Implementation
- **Network Interface Validation**: Use POSIX getifaddrs() for interface detection
- **Timing**: std::chrono::steady_clock for all timestamps (nanosecond precision)
- **Byte Order**: Little-endian only (no byte swapping, compile-time assertion)
- **Platform Detection**: CMake compile-time detection with minimal runtime checks
- **Supported Platforms**: Linux (glibc 2.17+) and macOS (10.12+)
- **No External Dependencies**: Implement platform features using standard APIs

### 19. Logging Requirements
- **Logging Approach**: Callback-based system (application controls output)
- **Log Levels**: Standard levels (TRACE, DEBUG, INFO, WARNING, ERROR)
- **Performance**: DEBUG/TRACE compiled out in Release builds (zero overhead)
- **Output Control**: Application decides log destination via callback registration
- **Log Content**: All events (connection, data transfer, errors, performance metrics)
- **No Dependencies**: No external logging library required

### 20. Error Handling Details
- **Sequence Gaps**: Request retransmission if possible, otherwise continue processing
- **Buffer Overflow**: Drop newest messages and report via logging
- **Connection Loss**: Immediate reconnection attempts with configurable retry count
- **Additional Error Types**: DNS resolution, port binding, malformed config, thread creation failures
- **Error Context**: Include stack traces (debug builds), errno codes, human-readable timestamps
- **Timestamp Format**: Human-friendly format (e.g., "2025-09-30-10:30:21")
- **Error Reporting**: Use logging callbacks (no separate error callback system)

### 21. Build System Requirements
- **Build System**: CMake 3.15+ for modern CMake features
- **Dependency Management**: find_package() for system libraries, FetchContent fallback if not found
- **Library Types**: Both static (.a) and shared (.so/.dylib) library options via CMake flag
- **Installation Structure**: 
  - Headers: `/usr/local/include/delila/net/`
  - Libraries: `/usr/local/lib/`
  - pkg-config: `/usr/local/lib/pkgconfig/delila_net.pc`
- **External Dependencies**: ZeroMQ, LZ4, xxHash, Protocol Buffers with version requirements
- **Compiler Flags**: -O2 optimization, full warnings, C++17 standard enforcement

## Non-Requirements
- Windows support not needed
- Encryption/authentication not needed at this layer
- API design (separate library)
- Module lifecycle management (handled by separate module control library)
- System-wide coordination and failure response (application-level concerns)