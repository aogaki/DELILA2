# DELILA2 Networking Library Design

## Overview

This document describes the design and architecture of the DELILA2 networking library, a high-performance data transport layer for DAQ systems built on ZeroMQ.

## Architecture Overview

```
┌─────────────────────┐
│ Module Control      │  ← System coordination, lifecycle management
│ Library             │    (separate library, not part of networking)
├─────────────────────┤
│   Application       │  ← DAQ modules (fetcher, merger, recorder, etc.)
├─────────────────────┤
│   DELILA2 Networking Library (this library)
├─────────────────────┤
│   Serialization     │  ← Binary format, compression, encoding/decoding
├─────────────────────┤
│   Message Layer     │  ← Priority queues, message types, routing
├─────────────────────┤
│   Transport Layer   │  ← ZeroMQ connections, TCP/IPC selection
│   (ZeroMQ)          │
└─────────────────────┘
```

**Scope of this library:** Pure networking functionality - data transport, serialization, and connection management. System-wide coordination and module lifecycle management are handled by higher-level libraries.

## Core Components

### 1. Protocol Constants

```cpp
namespace DELILA::Net {

// Protocol constants
constexpr uint64_t MAGIC_NUMBER = 0x44454C494C413200ULL; // "DELILA2\0"
constexpr uint32_t CURRENT_FORMAT_VERSION = 1;
constexpr uint32_t HEADER_SIZE = 64;

// Performance limits
constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024;      // 1MB
constexpr size_t MAX_EVENTS_PER_MESSAGE = 10000;      // Configurable limit
constexpr size_t MIN_MESSAGE_SIZE = 100 * 1024;       // 100KB

// Default buffer settings (for 128GB system with 100KB-1MB messages)
constexpr int DEFAULT_HIGH_WATER_MARK = 2000;         // ~1GB buffer at ZMQ level
constexpr int DEFAULT_RECEIVE_BUFFER_SIZE = 1000;     // ~500MB buffer at application level

// Error thresholds
constexpr uint32_t DEFAULT_GAP_ALERT_THRESHOLD = 10;   // gaps per minute
constexpr int DEFAULT_TIMEOUT_MS = 5000;
constexpr int DEFAULT_RETRY_ATTEMPTS = -1;            // infinite
constexpr int DEFAULT_INITIAL_RETRY_MS = 100;
constexpr int DEFAULT_MAX_RETRY_MS = 30000;

// Compression settings
constexpr int DEFAULT_COMPRESSION_LEVEL = 1;          // LZ4 fast compression

}  // namespace DELILA::Net
```

### 2. Serialization Module

The serialization module handles encoding/decoding of data before network transmission.

#### 1.1 Binary Data Serialization

**Header Structure** (uncompressed, fixed size):
```cpp
namespace DELILA::Net {

// All multi-byte integers use little-endian byte order
struct BinaryDataHeader {
    uint64_t magic_number;      // 8 bytes: 0x44454C494C413200 ("DELILA2\0")
    uint64_t sequence_number;   // 8 bytes: for gap detection (starts at 0)
    uint32_t format_version;    // 4 bytes: binary format version (starts at 1)
    uint32_t header_size;       // 4 bytes: size of this header (always 64)
    uint32_t event_count;       // 4 bytes: number of events in payload
    uint32_t uncompressed_size; // 4 bytes: size before compression
    uint32_t compressed_size;   // 4 bytes: size after compression (equals uncompressed_size if no compression)
    uint32_t checksum;          // 4 bytes: xxHash32 of payload data
    uint64_t timestamp;         // 8 bytes: Unix timestamp in nanoseconds since epoch
    uint32_t reserved[4];       // 16 bytes: future expansion
};  // Total: 64 bytes

}  // namespace DELILA::Net
```

**Byte Order**:
- All multi-byte integers use little-endian byte order
- Optimized for x86 and ARM architectures (no byte swapping needed)
- Direct memory access possible on target platforms
- Compile-time assertion to ensure platform is little-endian

**Compression Detection**:
- If `compressed_size == uncompressed_size`: Data is not compressed
- If `compressed_size < uncompressed_size`: Data is LZ4 compressed
- Body size to read from network is always `compressed_size`

**Payload Structure**:
- Array of serialized EventData structures (may be LZ4-compressed)
- EventData defined in EventData.hpp as common DAQ data structure
- Since EventData contains std::vector, custom serialization required
- All multi-byte integers use little-endian byte order
- Direct binary serialization using memcpy for POD fields
- No special memory alignment requirements

**EventData Serialization Format** (per event in payload):
```cpp
// Fixed-size portion (44 bytes, little-endian, packed - no padding)
// Based on DELILA::Digitizer::EventData from include/delila/digitizer/EventData.hpp
// CRITICAL: Fields must be serialized in ALPHABETICAL ORDER for consistency

// Alphabetical order of EventData fields:
// 1. analogProbe1Type, analogProbe2Type, channel, digitalProbe1Type, digitalProbe2Type, 
//    digitalProbe3Type, digitalProbe4Type, downSampleFactor, energy, energyShort, 
//    flags, module, timeResolution, timeStampNs, waveformSize

struct SerializedEventDataHeader {
    uint8_t analogProbe1Type;     // 1 byte
    uint8_t analogProbe2Type;     // 1 byte  
    uint8_t channel;              // 1 byte
    uint8_t digitalProbe1Type;    // 1 byte
    uint8_t digitalProbe2Type;    // 1 byte
    uint8_t digitalProbe3Type;    // 1 byte
    uint8_t digitalProbe4Type;    // 1 byte
    uint8_t downSampleFactor;     // 1 byte
    uint16_t energy;              // 2 bytes
    uint16_t energyShort;         // 2 bytes
    uint64_t flags;               // 8 bytes (status flags: pileup, trigger lost, etc.)
    uint8_t module;               // 1 byte
    uint8_t timeResolution;       // 1 byte
    double timeStampNs;           // 8 bytes
    uint32_t waveformSize;        // 4 bytes (FIXED SIZE, not size_t)
};  // Total: 44 bytes (no padding, packed)

// Variable-size waveform data (only if waveformSize > 0)
// All vectors have same size = waveformSize
// Serialized in alphabetical order by field name:
// 1. analogProbe1: waveformSize * sizeof(int32_t) bytes
// 2. analogProbe2: waveformSize * sizeof(int32_t) bytes  
// 3. digitalProbe1: waveformSize * sizeof(uint8_t) bytes
// 4. digitalProbe2: waveformSize * sizeof(uint8_t) bytes
// 5. digitalProbe3: waveformSize * sizeof(uint8_t) bytes
// 6. digitalProbe4: waveformSize * sizeof(uint8_t) bytes

// Total EventData size = 44 + waveformSize * (4 + 4 + 1 + 1 + 1 + 1) = 44 + waveformSize * 12 bytes
```

**Serializer Class**:
```cpp
namespace DELILA::Net {

class BinarySerializer {
private:
    // Configuration
    NetworkConfig config_;
    uint32_t format_version_ = 1;
    
    // State
    std::atomic<uint64_t> sequence_number_{0};
    
    // Compression context (reused for performance)
    mutable std::unique_ptr<LZ4_stream_t> compression_ctx_;
    
public:
    // Constructor with configuration
    explicit BinarySerializer(const NetworkConfig& config = NetworkConfig{});
    ~BinarySerializer();
    
    // Configuration methods
    void setConfig(const NetworkConfig& config);
    const NetworkConfig& getConfig() const { return config_; }
    
    // Compression control methods
    void enableCompression(bool enable) { config_.enable_compression = enable; }
    void setCompressionLevel(int level) { config_.compression_level = level; }
    bool isCompressionEnabled() const { return config_.enable_compression; }
    int getCompressionLevel() const { return config_.compression_level; }
    
    // Single EventData serialization
    Result<std::vector<uint8_t>> encodeEvent(const DELILA::Digitizer::EventData& event);
    Result<DELILA::Digitizer::EventData> decodeEvent(const uint8_t* data, size_t size);
    
    // Batch EventData serialization (optimized for network transmission)
    // Behavior: 
    // - Empty vector returns empty payload (success, not error)
    // - No maximum events limit enforced
    // - Compression applied based on level and enable flag
    // - Compression used if: enable_compression=true AND compression_level > 0
    Result<std::vector<uint8_t>> encode(const std::vector<DELILA::Digitizer::EventData>& events);
    Result<std::vector<DELILA::Digitizer::EventData>> decode(const std::vector<uint8_t>& data);
    
    // Low-level methods for custom usage
    Result<size_t> calculateEventSize(const DELILA::Digitizer::EventData& event) const;
    Result<size_t> serializeEventToBuffer(const DELILA::Digitizer::EventData& event, uint8_t* buffer, size_t buffer_size);
    Result<size_t> deserializeEventFromBuffer(const uint8_t* buffer, size_t buffer_size, DELILA::Digitizer::EventData& event);
    
    // Get next sequence number
    uint64_t getNextSequence() { return sequence_number_++; }
    
    // Thread safety: NOT thread-safe
    // Each thread MUST have its own BinarySerializer instance
    // Sharing instances between threads will cause data corruption
};

}  // namespace DELILA::Net
```

#### 1.2 Control Message Serialization

**Protocol Buffers** for all control messages defined in `proto/control_messages.proto`:

**Complete Message Specifications**:
- **Module Identification**: All messages include module_id and instance_id
- **Correlation Tracking**: All messages include correlation_id for request/response tracking
- **Timestamps**: All messages include nanosecond-precision timestamps
- **No Priority Levels**: Messages handled in order of arrival (no priority queuing)

**Message Types**:

1. **State Change Messages**:
   ```cpp
   // StateChangeCommand: Request state transition
   // - 5-second default timeout for transitions  
   // - Correlation ID for tracking responses
   
   // StateChangeResponse: Confirm state transition
   // - Includes both previous_state and current_state
   // - Success/failure indication with error details
   ```

2. **Heartbeat Messages**:
   ```cpp
   // HeartbeatMessage: Module alive indication
   // - Current module state
   // - Correlation ID for tracking responses
   ```

3. **Status Reports**:
   ```cpp
   // StatusReport: Network performance metrics
   // - bytes_sent, bytes_received, messages_sent, messages_received
   // - current_send_rate_mbps, current_receive_rate_mbps  
   // - connection_errors, sequence_gaps, messages_dropped
   // - Timestamp for metric collection time
   ```

4. **Error Notifications**:
   ```cpp
   // ErrorNotification: Error reporting with recovery guidance
   // - Complete error codes (17 types) matching Error::Code enum
   // - Error categories: NETWORK, CONFIGURATION, SERIALIZATION, SYSTEM, PROTOCOL
   // - suggested_recovery_action field for automated recovery
   // - No stack trace information (for performance)
   ```

**Protocol Buffer Integration**:
```cpp
namespace DELILA::Net {

class ProtocolBufferSerializer {
public:
    // Serialize control messages to binary format
    static Result<std::vector<uint8_t>> serialize(const delila::net::control::StateChangeCommand& msg);
    static Result<std::vector<uint8_t>> serialize(const delila::net::control::StateChangeResponse& msg);
    static Result<std::vector<uint8_t>> serialize(const delila::net::control::HeartbeatMessage& msg);
    static Result<std::vector<uint8_t>> serialize(const delila::net::control::StatusReport& msg);
    static Result<std::vector<uint8_t>> serialize(const delila::net::control::ErrorNotification& msg);
    
    // Deserialize control messages from binary format
    static Result<delila::net::control::StateChangeCommand> deserializeStateCommand(const std::vector<uint8_t>& data);
    static Result<delila::net::control::StateChangeResponse> deserializeStateResponse(const std::vector<uint8_t>& data);
    static Result<delila::net::control::HeartbeatMessage> deserializeHeartbeat(const std::vector<uint8_t>& data);
    static Result<delila::net::control::StatusReport> deserializeStatusReport(const std::vector<uint8_t>& data);
    static Result<delila::net::control::ErrorNotification> deserializeErrorNotification(const std::vector<uint8_t>& data);
    
    // Generate correlation IDs
    static std::string generateCorrelationId();
    
    // Set common message fields
    static void setCommonFields(google::protobuf::Message& msg, const std::string& module_id, 
                               const std::string& instance_id, const std::string& correlation_id = "");
};

}  // namespace DELILA::Net
```

#### 1.3 Histogram Data Serialization

- JSON format for flexibility
- Optional compression for large histograms

### 2. Message Layer

The message layer provides high-level messaging patterns on top of ZeroMQ.

#### 2.1 Message Types

```cpp
namespace DELILA::Net {

enum class MessageType : uint8_t {
    DATA_BINARY = 0x01,
    DATA_HISTOGRAM = 0x02,
    CONTROL_HEARTBEAT = 0x10,
    CONTROL_STATE = 0x11,
    CONTROL_STATUS = 0x12,
    CONTROL_ERROR = 0x13
};

}  // namespace DELILA::Net
```

#### 2.2 Message Priority Queue

- Internal priority queue for message ordering
- Experimental data gets highest priority
- Separate queues per message type

### 3. Transport Layer

#### 3.1 Connection Management

```cpp
namespace DELILA::Net {

class Connection {
public:
    // Connection types matching ZMQ patterns
    enum Type {
        PUBLISHER,   // PUB socket
        SUBSCRIBER,  // SUB socket
        PUSHER,      // PUSH socket
        PULLER,      // PULL socket
        DEALER,      // DEALER socket
        ROUTER,      // ROUTER socket
        CLIENT,      // REQ socket
        SERVER       // REP socket
    };
    
    // Transport types for optimal performance
    enum TransportType {
        AUTO,        // Automatically choose IPC for localhost, TCP for remote
        TCP,         // TCP transport for multi-machine setup
        IPC          // IPC transport for single-machine setup (faster)
    };
    
    // High-level API for control messages (convenience)
    // Behavior: Non-blocking operations
    // - send(): Returns immediately, does not wait for transmission
    // - receive(): Returns immediately, Error::TIMEOUT if no message available
    // - Auto-retry on transient failures (connection loss, temporary errors)
    // - Log errors and continue operation when possible
    Result<void> send(const Message& msg);
    Result<Message> receive();
    
    // Low-level API for binary data (performance)
    // Behavior: Non-blocking operations  
    // - sendRaw(): Returns immediately after queuing data for transmission
    // - receiveRaw(): Returns immediately, Error::TIMEOUT if no data available
    // - Auto-retry on transient failures with exponential backoff
    // - Log errors and continue operation when possible
    Result<void> sendRaw(const std::vector<uint8_t>& data);
    Result<std::vector<uint8_t>> receiveRaw();
    
    // Connection management with transport selection
    Result<void> connect(const std::string& endpoint, TransportType transport = AUTO);
    Result<void> bind(const std::string& endpoint, TransportType transport = AUTO);
    void bindToInterface(const std::string& interface);
    
    // Configuration and state
    bool isConnected() const;
    void setHighWaterMark(int hwm);
    void setTimeout(int timeout_ms);
    TransportType getActiveTransport() const;
    
private:
    // RAII ZeroMQ socket wrapper
    class ZMQSocketWrapper {
        std::unique_ptr<zmq::socket_t> socket_;
        zmq::context_t& context_;
        int socket_type_;
        
    public:
        ZMQSocketWrapper(zmq::context_t& ctx, int type) 
            : context_(ctx), socket_type_(type) {
            socket_ = std::make_unique<zmq::socket_t>(ctx, type);
            configureSocket();
        }
        
        ~ZMQSocketWrapper() {
            if (socket_) {
                socket_->close();
            }
        }
        
        zmq::socket_t& get() { return *socket_; }
        
    private:
        void configureSocket() {
            // Set socket options for reliability and performance
            int hwm = DEFAULT_HIGH_WATER_MARK;
            socket_->setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
            socket_->setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
            
            int timeout = 5000;  // 5 second timeout
            socket_->setsockopt(ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
            socket_->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
            
            // TCP Keepalive settings
            int keepalive = 1;
            socket_->setsockopt(ZMQ_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
            int keepalive_idle = 300;  // 5 minutes
            socket_->setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, &keepalive_idle, sizeof(keepalive_idle));
            int keepalive_intvl = 30;  // 30 seconds
            socket_->setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, &keepalive_intvl, sizeof(keepalive_intvl));
        }
    };
    
    std::unique_ptr<ZMQSocketWrapper> socket_;
    
    // Transport selection algorithm
    TransportType selectTransport(const std::string& endpoint, TransportType requested) {
        if (requested != AUTO) return requested;
        
        // Parse endpoint to determine if it's local
        if (endpoint.find("localhost") != std::string::npos ||
            endpoint.find("127.0.0.1") != std::string::npos ||
            endpoint.empty()) {
            return IPC;
        }
        return TCP;
    }
    
    // Convert endpoint format based on transport
    std::string formatEndpoint(const std::string& endpoint, TransportType transport) {
        if (transport == IPC) {
            // Convert to IPC format: ipc:///tmp/delila_<name>
            size_t pos = endpoint.find_last_of(":");
            if (pos != std::string::npos) {
                std::string name = endpoint.substr(pos + 1);
                return "ipc:///tmp/delila_" + name;
            }
            return "ipc:///tmp/delila_default";
        } else {
            // Ensure TCP format: tcp://ip:port
            if (endpoint.find("tcp://") == 0) {
                return endpoint;
            }
            return "tcp://" + endpoint;
        }
    }
    
    // Thread safety: NOT thread-safe
    // - Each thread MUST own its Connection instances
    // - Never share Connection objects between threads
    // - ZeroMQ sockets are not thread-safe by design
    // - Create one Connection per thread for parallel operations
};

}  // namespace DELILA::Net
```

**Complete Connection API Specification:**

```cpp
namespace DELILA::Net {

// Forward declarations
class Message;
class ConnectionConfig;
class ConnectionStats;

class Connection {
public:
    // Constructor and destructor
    explicit Connection(Type type, const ConnectionConfig& config = {});
    virtual ~Connection();
    
    // Delete copy constructor/assignment (not copyable)
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    
    // Move constructor/assignment (movable)
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    
    // Connection lifecycle management
    Result<void> connect(const std::string& endpoint, TransportType transport = AUTO);
    Result<void> bind(const std::string& endpoint, TransportType transport = AUTO);
    Result<void> disconnect();
    void close();
    
    // Message sending/receiving
    Result<void> send(const Message& msg);
    Result<Message> receive();
    Result<void> sendRaw(const std::vector<uint8_t>& data);
    Result<std::vector<uint8_t>> receiveRaw();
    
    // Batch operations for performance
    Result<void> sendBatch(const std::vector<Message>& messages);
    Result<std::vector<Message>> receiveBatch(size_t max_messages = 100);
    
    // Configuration and state management
    bool isConnected() const;
    bool isBound() const;
    void setHighWaterMark(int send_hwm, int recv_hwm);
    void setTimeout(int send_timeout_ms, int recv_timeout_ms);
    void setLinger(int linger_ms);
    void bindToInterface(const std::string& interface);
    
    // Pattern-specific methods
    // PUB/SUB pattern
    Result<void> subscribe(const std::string& topic);    // SUB only
    Result<void> unsubscribe(const std::string& topic);  // SUB only
    
    // DEALER/ROUTER pattern  
    Result<void> setIdentity(const std::string& identity);
    std::string getIdentity() const;
    
    // Statistics and monitoring
    ConnectionStats getStats() const;
    void resetStats();
    TransportType getActiveTransport() const;
    std::string getEndpoint() const;
    
    // Event callbacks (optional)
    using ConnectCallback = std::function<void(const std::string& endpoint)>;
    using DisconnectCallback = std::function<void(const std::string& endpoint, const Error& reason)>;
    using ErrorCallback = std::function<void(const Error& error)>;
    
    void setConnectCallback(ConnectCallback callback);
    void setDisconnectCallback(DisconnectCallback callback);
    void setErrorCallback(ErrorCallback callback);

private:
    // Implementation details hidden
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Connection configuration structure
struct ConnectionConfig {
    int send_timeout_ms = 5000;
    int recv_timeout_ms = 5000;
    int send_hwm = 2000;
    int recv_hwm = 2000;
    int linger_ms = 1000;
    bool tcp_keepalive = true;
    int keepalive_idle_sec = 300;
    int keepalive_interval_sec = 30;
    int keepalive_count = 3;
    int reconnect_interval_ms = 100;
    int max_reconnect_interval_ms = 30000;
    int reconnect_attempts = -1;  // -1 = infinite
    std::string bind_interface;   // e.g., "eth0", empty = all interfaces
};

// Connection statistics
struct ConnectionStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
    uint64_t connection_attempts = 0;
    uint64_t connection_failures = 0;
    uint64_t reconnections = 0;
    std::chrono::steady_clock::time_point connected_since;
    std::chrono::milliseconds total_downtime{0};
    double current_send_rate_mbps = 0.0;
    double current_recv_rate_mbps = 0.0;
};

}  // namespace DELILA::Net
```

#### 3.2 Message Structure

```cpp
namespace DELILA::Net {

// Message type enumeration for protocol versioning
enum class MessageType : uint32_t {
    BINARY_DATA = 0x00000001,          // EventData binary serialization
    CONTROL_MESSAGE = 0x00000002,      // Protocol Buffer control messages
    JSON_DATA = 0x00000003,            // JSON histogram data
    HEARTBEAT = 0x00000004,            // Heartbeat/keepalive messages
    ERROR_REPORT = 0x00000005,         // Error reporting messages
    STATUS_UPDATE = 0x00000006,        // Status update messages
    RETRANSMISSION_REQUEST = 0x00000007, // Request for retransmission
    UNKNOWN = 0xFFFFFFFF
};

// ZeroMQ Multi-part Message Frame Structure
struct MessageFrames {
    // Frame 0: Routing envelope (ROUTER/DEALER patterns only)
    std::optional<std::vector<uint8_t>> routing_id;
    
    // Frame 1: Message header (always present)
    struct MessageHeader {
        uint32_t magic_number = 0x44454C4E;    // "DELN" for DELILA Network
        uint32_t protocol_version = 1;         // Protocol version
        MessageType message_type;              // Message type identifier
        uint32_t header_size = 32;             // Size of this header
        uint32_t payload_size;                 // Size of payload frame
        uint64_t sequence_number;              // Message sequence number
        uint64_t timestamp_ns;                 // Send timestamp (nanoseconds)
        uint32_t checksum;                     // CRC32 of header + payload
        uint32_t reserved = 0;                 // Reserved for future use
    } header;  // Total: 32 bytes
    
    // Frame 2: Message payload (content depends on message_type)
    std::vector<uint8_t> payload;
    
    // Frame 3: Optional metadata (JSON string)
    std::optional<std::string> metadata;
};

// Message container for all network communications
struct Message {
    MessageFrames frames;            // Multi-part ZeroMQ frames
    uint64_t receive_timestamp;      // Local receive timestamp
    std::string source_endpoint;     // ZeroMQ endpoint info
    std::string connection_id;       // Connection identifier
    
    // Convenience methods
    MessageType getType() const { return frames.header.message_type; }
    uint64_t getSequenceNumber() const { return frames.header.sequence_number; }
    uint64_t getSendTimestamp() const { return frames.header.timestamp_ns; }
    size_t getPayloadSize() const { return frames.payload.size(); }
    
    // Validation
    bool isValid() const;
    bool verifyChecksum() const;
    
    // Serialization for wire format
    std::vector<std::vector<uint8_t>> toZmqFrames() const;
    static Result<Message> fromZmqFrames(const std::vector<std::vector<uint8_t>>& zmq_frames);
};

// Protocol-specific payload structures
namespace Protocol {

// Binary data payload (EventData serialization)
struct BinaryDataPayload {
    // Uses existing BinarySerializer output
    // Header: BinaryDataHeader (64 bytes)
    // Body: LZ4-compressed EventData array
};

// Control message payload (Protocol Buffers)
struct ControlMessagePayload {
    // Defined in control_messages.proto
    std::vector<uint8_t> protobuf_data;
};

// JSON data payload (histogram data)
struct JsonDataPayload {
    std::string json_string;
    std::string schema_version = "1.0";
};

// Heartbeat payload
struct HeartbeatPayload {
    uint64_t sender_timestamp_ns;
    uint32_t sender_sequence_number;
    std::string sender_module_id;
    uint8_t health_status = 0;  // 0=OK, 1=WARNING, 2=ERROR
};

// Error report payload
struct ErrorReportPayload {
    uint32_t error_code;
    std::string error_message;
    std::string error_context;
    uint64_t error_timestamp_ns;
    std::string module_id;
};

}  // namespace Protocol

// Message builder for different patterns
class MessageBuilder {
public:
    // Build binary data message
    static Message buildBinaryDataMessage(const std::vector<uint8_t>& binary_data,
                                        uint64_t sequence_number);
    
    // Build control message
    static Message buildControlMessage(const std::vector<uint8_t>& protobuf_data,
                                     uint64_t sequence_number);
    
    // Build heartbeat message
    static Message buildHeartbeatMessage(const std::string& module_id,
                                       uint64_t sequence_number);
    
    // Build error report
    static Message buildErrorReport(const Error& error,
                                  const std::string& module_id,
                                  uint64_t sequence_number);
    
private:
    static uint32_t calculateChecksum(const MessageFrames::MessageHeader& header,
                                    const std::vector<uint8_t>& payload);
    static uint64_t getCurrentTimestampNs();
};

// Message pattern routing
class MessageRouter {
public:
    // Topic generation for PUB/SUB
    static std::string generateTopic(MessageType type, const std::string& module_id = "");
    
    // Identity generation for DEALER/ROUTER
    static std::string generateIdentity(const std::string& module_id, int instance_id);
    
    // Route message based on pattern
    static std::vector<std::string> getDestinations(const Message& msg,
                                                  Connection::Type pattern);
};

// Flow control and backpressure
class FlowController {
public:
    struct FlowControlParams {
        size_t max_queue_size = 2000;
        size_t high_water_mark = 1800;   // 90% of max
        size_t low_water_mark = 1000;    // 50% of max
        int backpressure_timeout_ms = 100;
    };
    
    bool shouldAcceptMessage(size_t current_queue_size) const;
    std::chrono::milliseconds getBackpressureDelay(size_t queue_size) const;
    
private:
    FlowControlParams params_;
};

}  // namespace DELILA::Net
```

#### 3.3 Module Base Classes

```cpp
namespace DELILA::Net {

// Base class for all modules
class NetworkModule {
protected:
    // Shared ZMQ context (thread-safe)
    std::shared_ptr<zmq::context_t> context_;
    
    // Per-thread connections (not shared)
    thread_local std::unordered_map<std::string, Connection> connections_;
    
    // Lock-free single producer/single consumer queue
    // Producer: receiver thread, Consumer: processing thread
    struct LockFreeQueue {
        static constexpr size_t QUEUE_SIZE = 2048;  // Power of 2 for fast modulo
        std::array<Message, QUEUE_SIZE> buffer;
        alignas(64) std::atomic<size_t> write_index{0};  // Cache line aligned
        alignas(64) std::atomic<size_t> read_index{0};   // Cache line aligned
        
        bool push(Message&& msg);  // Returns false if full
        bool pop(Message& msg);    // Returns false if empty
    } receive_queue_;
    
    std::thread receiver_thread_;
    
    // Lock-free monitoring counters
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> bytes_received_{0};
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_received_{0};
    
public:
    virtual void handleMessage(const Message& msg) = 0;
    virtual void onConnectionLost(const std::string& endpoint) = 0;
};

}  // namespace DELILA::Net
```

### 4. Error Handling

#### 4.1 Error Types and Result Pattern

```cpp
namespace DELILA::Net {

// Error information structure
struct Error {
    enum Code {
        SUCCESS = 0,
        CONNECTION_LOST,
        TIMEOUT,
        BUFFER_FULL,
        INVALID_MESSAGE,
        SEQUENCE_GAP,
        COMPRESSION_FAILED,
        CHECKSUM_MISMATCH,
        INVALID_HEADER,
        VERSION_MISMATCH,
        SYSTEM_ERROR,           // Platform-specific system call failures
        INVALID_INTERFACE,      // Network interface validation failed
        DNS_RESOLUTION_FAILED,  // DNS lookup failure
        PORT_BIND_FAILED,       // Port already in use or binding failure
        INVALID_CONFIG,         // Malformed configuration
        THREAD_CREATION_FAILED, // Thread creation/management failure
        RETRANSMISSION_FAILED   // Failed to request retransmission
    };
    
    Code code;
    std::string message;
    std::optional<std::string> endpoint;    // Which connection failed
    std::optional<int> system_errno;        // System errno for system call failures
    std::string timestamp;                  // Human-readable timestamp (e.g., "2025-09-30-10:30:21")
    
#ifndef NDEBUG
    std::vector<std::string> stack_trace;   // Stack trace in debug builds
#endif
    
    // Constructor with current timestamp
    Error(Code c, const std::string& msg, const std::string& ep = "", int errno_val = 0);
    
    // Helper to format timestamp
    static std::string getCurrentTimestamp();
};

// Result type for error handling without exceptions
template<typename T>
using Result = std::variant<T, Error>;

// Helper for void returns
using Status = Result<std::monostate>;

// Helper functions
template<typename T>
bool isOk(const Result<T>& result) {
    return std::holds_alternative<T>(result);
}

template<typename T>
const T& getValue(const Result<T>& result) {
    return std::get<T>(result);
}

template<typename T>
const Error& getError(const Result<T>& result) {
    return std::get<Error>(result);
}

// Error class implementation
inline Error::Error(Code c, const std::string& msg, const std::string& ep, int errno_val)
    : code(c), message(msg), timestamp(getCurrentTimestamp()) {
    if (!ep.empty()) {
        endpoint = ep;
    }
    if (errno_val != 0) {
        system_errno = errno_val;
        // Append system error description to message
        message += " (errno: " + std::to_string(errno_val) + " - " + std::strerror(errno_val) + ")";
    }
    
#ifndef NDEBUG
    // Capture stack trace in debug builds
    // Note: Actual implementation would use platform-specific stack trace capture
    stack_trace.push_back("Stack trace capture not implemented in design document");
#endif
}

inline std::string Error::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto tm_now = *std::localtime(&time_t_now);
    
    // Format: "2025-09-30-10:30:21"
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H:%M:%S", &tm_now);
    return std::string(buffer);
}

}  // namespace DELILA::Net
```

**Detailed Recovery Strategies:**

```cpp
namespace DELILA::Net {

// Recovery strategy configuration
struct RecoveryConfig {
    int initial_retry_delay_ms = 100;
    int max_retry_delay_ms = 30000;
    double backoff_multiplier = 2.0;
    int max_retry_attempts = -1;  // -1 = infinite
    bool enable_connection_health_monitoring = true;
    int heartbeat_interval_ms = 30000;
    int dead_peer_timeout_ms = 90000;
};

// Recovery state machine
class ConnectionRecovery {
public:
    enum State {
        CONNECTED,
        DISCONNECTED,
        RECONNECTING,
        FAILED
    };
    
    State getCurrentState() const;
    void handleConnectionLost(const Error& reason);
    void handleReconnectSuccess();
    void handleReconnectFailure(const Error& reason);
    
private:
    State state_ = DISCONNECTED;
    std::chrono::steady_clock::time_point last_attempt_;
    int current_delay_ms_;
    int attempt_count_;
    RecoveryConfig config_;
};

// Error-specific recovery actions
class ErrorRecoveryManager {
public:
    enum RecoveryAction {
        RETRY_IMMEDIATELY,
        RETRY_WITH_BACKOFF,
        RECONNECT,
        RESET_CONNECTION,
        REPORT_AND_CONTINUE,
        FAIL_PERMANENTLY
    };
    
    RecoveryAction getRecoveryAction(Error::Code error_code) const {
        switch (error_code) {
            case Error::CONNECTION_LOST:
                return RECONNECT;
            case Error::TIMEOUT:
                return RETRY_WITH_BACKOFF;
            case Error::BUFFER_FULL:
                return RETRY_WITH_BACKOFF;
            case Error::INVALID_MESSAGE:
                return REPORT_AND_CONTINUE;
            case Error::SEQUENCE_GAP:
                return REPORT_AND_CONTINUE;  // Log and continue
            case Error::COMPRESSION_FAILED:
                return REPORT_AND_CONTINUE;
            case Error::CHECKSUM_MISMATCH:
                return REPORT_AND_CONTINUE;  // Data corruption, log and continue
            case Error::INVALID_HEADER:
                return REPORT_AND_CONTINUE;
            case Error::VERSION_MISMATCH:
                return FAIL_PERMANENTLY;
            case Error::SYSTEM_ERROR:
                return RETRY_WITH_BACKOFF;
            case Error::INVALID_INTERFACE:
                return FAIL_PERMANENTLY;
            case Error::DNS_RESOLUTION_FAILED:
                return RETRY_WITH_BACKOFF;
            case Error::PORT_BIND_FAILED:
                return FAIL_PERMANENTLY;
            case Error::INVALID_CONFIG:
                return FAIL_PERMANENTLY;
            case Error::THREAD_CREATION_FAILED:
                return FAIL_PERMANENTLY;
            case Error::RETRANSMISSION_FAILED:
                return REPORT_AND_CONTINUE;
            default:
                return RETRY_WITH_BACKOFF;
        }
    }
    
    // Calculate next retry delay with exponential backoff
    std::chrono::milliseconds calculateRetryDelay(int attempt_count, const RecoveryConfig& config) {
        int delay = config.initial_retry_delay_ms;
        for (int i = 0; i < attempt_count && delay < config.max_retry_delay_ms; ++i) {
            delay = static_cast<int>(delay * config.backoff_multiplier);
        }
        return std::chrono::milliseconds(std::min(delay, config.max_retry_delay_ms));
    }
};

}  // namespace DELILA::Net
```

#### 4.2 Sequence Gap Detection

```cpp
namespace DELILA::Net {

class SequenceTracker {
    uint64_t expected_sequence_ = 0;
    uint64_t gaps_detected_ = 0;
    std::chrono::steady_clock::time_point last_gap_time_;
    
    // Retransmission callback type
    using RetransmissionCallback = std::function<bool(uint64_t start_seq, uint64_t end_seq)>;
    RetransmissionCallback retransmission_callback_;
    
public:
    // Set callback for retransmission requests
    void setRetransmissionCallback(RetransmissionCallback callback) {
        retransmission_callback_ = std::move(callback);
    }
    
    // Check sequence and handle gaps
    Result<uint64_t> checkSequence(uint64_t sequence);  // Returns gap size or error
    bool shouldAlert() const;  // Based on gap frequency
    
    // Gap recovery strategies
    Result<void> handleSequenceGap(uint64_t expected, uint64_t received);
    bool requestRetransmission(uint64_t start_seq, uint64_t end_seq);
};

}  // namespace DELILA::Net
```

#### 4.3 Connection Recovery

**Connection Loss Strategy**: Immediate reconnection attempts (no delay)
- **Initial Strategy**: Try several immediate reconnection attempts
- **Retry Count**: Configurable (default: 5 immediate attempts)
- **Fallback**: If immediate attempts fail, switch to exponential backoff
- **Exponential Backoff**: Initial 100ms, maximum 30s intervals
- **Maximum Attempts**: Configurable (default: infinite with backoff)

```cpp
namespace DELILA::Net {

class ConnectionRecovery {
private:
    int immediate_retry_count_ = 5;      // Immediate retry attempts
    int exponential_retry_count_ = -1;   // Infinite with backoff
    int current_immediate_attempts_ = 0;
    int current_exponential_attempts_ = 0;
    
    std::chrono::milliseconds initial_backoff_{100};
    std::chrono::milliseconds max_backoff_{30000};
    std::chrono::milliseconds current_backoff_{100};
    
public:
    // Recovery strategies
    enum class Strategy {
        IMMEDIATE,          // Try immediate reconnection
        EXPONENTIAL_BACKOFF // Use exponential backoff
    };
    
    Strategy getStrategy() const {
        return (current_immediate_attempts_ < immediate_retry_count_) 
               ? Strategy::IMMEDIATE 
               : Strategy::EXPONENTIAL_BACKOFF;
    }
    
    std::chrono::milliseconds getNextRetryDelay() {
        if (getStrategy() == Strategy::IMMEDIATE) {
            current_immediate_attempts_++;
            return std::chrono::milliseconds{0};  // No delay
        } else {
            current_exponential_attempts_++;
            auto delay = current_backoff_;
            current_backoff_ = std::min(current_backoff_ * 2, max_backoff_);
            return delay;
        }
    }
    
    void reset() {
        current_immediate_attempts_ = 0;
        current_exponential_attempts_ = 0;
        current_backoff_ = initial_backoff_;
    }
    
    bool shouldRetry() const {
        if (current_immediate_attempts_ < immediate_retry_count_) {
            return true;
        }
        return (exponential_retry_count_ == -1) || 
               (current_exponential_attempts_ < exponential_retry_count_);
    }
};

}  // namespace DELILA::Net
```

**Buffer Overflow Strategy**: Drop newest messages and report
- **Drop Policy**: When buffer is full, drop the newest incoming message
- **Reporting**: Log buffer overflow event with dropped message count
- **Recovery**: Continue normal operation after dropping message
- **Monitoring**: Track buffer usage and overflow frequency

```cpp
namespace DELILA::Net {

class BufferOverflowHandler {
private:
    std::atomic<uint64_t> messages_dropped_{0};
    std::atomic<uint64_t> last_overflow_time_{0};
    
public:
    enum class DropPolicy {
        DROP_NEWEST,   // Drop the incoming message (default)
        DROP_OLDEST    // Drop the oldest message in buffer
    };
    
    template<typename T>
    bool handleBufferOverflow(std::deque<T>& buffer, T&& new_message, 
                             DropPolicy policy = DropPolicy::DROP_NEWEST) {
        if (policy == DropPolicy::DROP_NEWEST) {
            // Drop the new message
            messages_dropped_++;
            last_overflow_time_ = getCurrentTimestampNs();
            
            DELILA_NET_LOG_WARNING(DELILA_NET_COMPONENT_TRANSPORT,
                                  "Buffer overflow: dropping newest message (total dropped: %lu)",
                                  messages_dropped_.load());
            return false;  // Message not added
        } else {
            // Drop oldest and add new
            if (!buffer.empty()) {
                buffer.pop_front();
                messages_dropped_++;
            }
            buffer.push_back(std::move(new_message));
            last_overflow_time_ = getCurrentTimestampNs();
            
            DELILA_NET_LOG_WARNING(DELILA_NET_COMPONENT_TRANSPORT,
                                  "Buffer overflow: dropped oldest message (total dropped: %lu)",
                                  messages_dropped_.load());
            return true;   // Message added
        }
    }
    
    uint64_t getDroppedCount() const { return messages_dropped_; }
    uint64_t getLastOverflowTime() const { return last_overflow_time_; }
};

}  // namespace DELILA::Net
```

### 5. Configuration Management

**Configuration Architecture**: Array-based configuration for multiple network entities per module.

```cpp
namespace DELILA::Net {

// Single network entity configuration (one per network connection/socket)
class NetworkEntityConfig {
public:
    // Required fields - no defaults, all must be specified
    std::string entity_name;            // Unique identifier for this network entity
    std::string endpoint;               // Connection endpoint (e.g., "tcp://192.168.1.100:5555")
    std::string socket_type;            // "PUBLISHER", "SUBSCRIBER", "PUSHER", etc.
    std::string transport_type;         // "TCP", "IPC", or "AUTO"
    
    // Buffer settings (required)
    int high_water_mark;                // Message buffer limit
    int receive_buffer_size;            // Application-level buffer size
    
    // Performance settings (required)
    int compression_level;              // LZ4 compression level (1-12)
    bool enable_compression;            // Enable/disable compression
    int timeout_ms;                     // Send/receive timeout
    
    // Error handling (required)
    int gap_alert_threshold;            // Sequence gaps per minute threshold
    int max_retry_attempts;             // Connection retry attempts (-1 = infinite)
    int initial_retry_interval_ms;      // Initial retry delay
    int max_retry_interval_ms;          // Maximum retry delay
    int immediate_retry_count;          // Immediate reconnection attempts
    
    // Network settings (required)
    std::string network_interface;      // Network interface binding (empty = default)
    
    // Validation - all fields required
    Result<void> validate() const;
    
    // Serialization
    std::string toJSON() const;
    static Result<NetworkEntityConfig> fromJSON(const std::string& json_str);
};

// Module-level configuration (array of network entities)
class NetworkModuleConfig {
public:
    std::vector<NetworkEntityConfig> entities;
    
    // Load configuration array from sources
    static Result<NetworkModuleConfig> loadFromFile(const std::string& filename);
    static Result<NetworkModuleConfig> loadFromJSON(const std::string& json_str);
    
    // Apply environment variable overrides to specific entity
    Result<void> applyEnvironmentOverrides(const std::string& entity_name);
    
    // Get configuration for specific entity
    Result<NetworkEntityConfig> getEntityConfig(const std::string& entity_name) const;
    
    // Validation
    Result<void> validate() const;
    
    // Serialization
    std::string toJSON() const;
};

}  // namespace DELILA::Net
```

#### Configuration File Format (JSON Array)

```json
[
  {
    "entity_name": "data_publisher",
    "endpoint": "tcp://0.0.0.0:5555",
    "socket_type": "PUBLISHER",
    "transport_type": "AUTO",
    "high_water_mark": 2000,
    "receive_buffer_size": 1000,
    "compression_level": 1,
    "enable_compression": true,
    "timeout_ms": 5000,
    "gap_alert_threshold": 10,
    "max_retry_attempts": -1,
    "initial_retry_interval_ms": 100,
    "max_retry_interval_ms": 30000,
    "immediate_retry_count": 5,
    "network_interface": ""
  },
  {
    "entity_name": "control_server",
    "endpoint": "ipc:///tmp/delila_control",
    "socket_type": "SERVER",
    "transport_type": "IPC",
    "high_water_mark": 100,
    "receive_buffer_size": 50,
    "compression_level": 1,
    "enable_compression": false,
    "timeout_ms": 10000,
    "gap_alert_threshold": 5,
    "max_retry_attempts": 10,
    "initial_retry_interval_ms": 50,
    "max_retry_interval_ms": 5000,
    "immediate_retry_count": 3,
    "network_interface": ""
  }
]
```

#### Environment Variable Overrides

**Naming Convention**: `DELILA_NET_<ENTITY_NAME>_<FIELD_NAME>`

```bash
# Override data_publisher endpoint
export DELILA_NET_DATA_PUBLISHER_ENDPOINT="tcp://192.168.1.200:5555"

# Override control_server timeout
export DELILA_NET_CONTROL_SERVER_TIMEOUT_MS=15000

# Override data_publisher buffer settings
export DELILA_NET_DATA_PUBLISHER_HIGH_WATER_MARK=4000
export DELILA_NET_DATA_PUBLISHER_RECEIVE_BUFFER_SIZE=2000

# Override network interface
export DELILA_NET_DATA_PUBLISHER_NETWORK_INTERFACE="eth1"

# Override transport type
export DELILA_NET_DATA_PUBLISHER_TRANSPORT_TYPE="TCP"
```

#### Configuration Validation Rules

```cpp
namespace DELILA::Net {

Result<void> NetworkEntityConfig::validate() const {
    // Entity name validation
    if (entity_name.empty()) {
        return Error{Error::INVALID_CONFIG, "entity_name is required"};
    }
    
    // Endpoint validation
    if (endpoint.empty()) {
        return Error{Error::INVALID_CONFIG, "endpoint is required for entity: " + entity_name};
    }
    
    // Parse and validate endpoint format
    auto parsed = EndpointParser::parse(endpoint);
    if (!isOk(parsed)) {
        return Error{Error::INVALID_CONFIG, "Invalid endpoint format: " + endpoint};
    }
    
    // Port range validation for TCP endpoints
    const auto& ep = getValue(parsed);
    if (ep.protocol == "tcp" && !ep.port.empty()) {
        int port = std::stoi(ep.port);
        if (port < 1 || port > 65535) {
            return Error{Error::INVALID_CONFIG, "Port must be 1-65535: " + ep.port};
        }
    }
    
    // Socket type validation
    static const std::set<std::string> valid_socket_types = {
        "PUBLISHER", "SUBSCRIBER", "PUSHER", "PULLER", 
        "DEALER", "ROUTER", "CLIENT", "SERVER"
    };
    if (valid_socket_types.find(socket_type) == valid_socket_types.end()) {
        return Error{Error::INVALID_CONFIG, "Invalid socket_type: " + socket_type};
    }
    
    // Transport type validation
    static const std::set<std::string> valid_transport_types = {"TCP", "IPC", "AUTO"};
    if (valid_transport_types.find(transport_type) == valid_transport_types.end()) {
        return Error{Error::INVALID_CONFIG, "Invalid transport_type: " + transport_type};
    }
    
    // Buffer size validation
    if (high_water_mark <= 0 || high_water_mark > 100000) {
        return Error{Error::INVALID_CONFIG, "high_water_mark must be 1-100000"};
    }
    
    if (receive_buffer_size <= 0 || receive_buffer_size > high_water_mark) {
        return Error{Error::INVALID_CONFIG, "receive_buffer_size must be 1 to high_water_mark"};
    }
    
    // Compression validation
    if (compression_level < 1 || compression_level > 12) {
        return Error{Error::INVALID_CONFIG, "compression_level must be 1-12"};
    }
    
    // Timeout validation
    if (timeout_ms < 100 || timeout_ms > 300000) {  // 100ms to 5 minutes
        return Error{Error::INVALID_CONFIG, "timeout_ms must be 100-300000"};
    }
    
    // Retry validation
    if (max_retry_attempts == 0 || max_retry_attempts < -1) {
        return Error{Error::INVALID_CONFIG, "max_retry_attempts must be -1 (infinite) or > 0"};
    }
    
    if (initial_retry_interval_ms < 10 || initial_retry_interval_ms > max_retry_interval_ms) {
        return Error{Error::INVALID_CONFIG, "invalid retry interval range"};
    }
    
    if (immediate_retry_count < 0 || immediate_retry_count > 100) {
        return Error{Error::INVALID_CONFIG, "immediate_retry_count must be 0-100"};
    }
    
    // Network interface validation (if specified)
    if (!network_interface.empty()) {
        auto interface_valid = NetworkInterface::isValidInterface(network_interface);
        if (!isOk(interface_valid) || !getValue(interface_valid)) {
            return Error{Error::INVALID_INTERFACE, "Network interface not found: " + network_interface};
        }
    }
    
    return std::monostate{};  // Success
}

}  // namespace DELILA::Net
```

**Complete NetworkConfig Structure for Phase 2:**

```cpp
namespace DELILA::Net {

// Global network configuration structure
struct NetworkConfig {
    // ZeroMQ Context Settings
    struct ContextConfig {
        int io_threads = 1;              // Number of I/O threads
        int max_sockets = 1024;          // Maximum number of sockets
        int socket_limit = 65536;        // Socket limit
        bool ipv6_enabled = false;       // IPv6 support (not required but configurable)
    } context;
    
    // Default Connection Settings (can be overridden per connection)
    struct DefaultConnectionConfig {
        int send_timeout_ms = 5000;
        int recv_timeout_ms = 5000;
        int send_hwm = 2000;
        int recv_hwm = 2000;
        int linger_ms = 1000;
        bool tcp_keepalive = true;
        int keepalive_idle_sec = 300;
        int keepalive_interval_sec = 30;
        int keepalive_count = 3;
        std::string bind_interface;      // Default interface to bind to
    } connection_defaults;
    
    // Recovery and Retry Settings
    struct RecoveryConfig {
        int initial_retry_delay_ms = 100;
        int max_retry_delay_ms = 30000;
        double backoff_multiplier = 2.0;
        int max_retry_attempts = -1;     // -1 = infinite
        int immediate_retry_count = 3;   // Immediate retries before backoff
        bool enable_connection_health_monitoring = true;
        int heartbeat_interval_ms = 30000;
        int dead_peer_timeout_ms = 90000;
    } recovery;
    
    // Message Processing Settings
    struct MessageConfig {
        size_t max_message_size = 1024 * 1024;      // 1MB max message
        size_t max_batch_size = 100;                // Max messages per batch
        bool enable_message_compression = true;
        int compression_level = 1;                  // LZ4 compression level
        bool enable_checksums = true;
        size_t min_compression_size = 1024;         // Don't compress small messages
    } message;
    
    // Threading Configuration
    struct ThreadConfig {
        int receiver_thread_priority = 0;       // Thread priority (-20 to 19 on Linux)
        size_t receiver_queue_size = 2000;      // Messages per receiver queue
        bool enable_thread_affinity = false;    // CPU affinity for threads
        std::vector<int> thread_cpu_affinity;   // CPU cores for thread affinity
        int thread_stack_size_kb = 0;           // 0 = system default
    } threading;
    
    // Performance Tuning
    struct PerformanceConfig {
        bool enable_zero_copy = true;            // Zero-copy optimizations
        bool enable_message_batching = true;     // Batch multiple messages
        int batch_timeout_ms = 10;               // Max time to wait for batch
        size_t send_buffer_size = 64 * 1024;     // OS send buffer size
        size_t recv_buffer_size = 64 * 1024;     // OS receive buffer size
        bool disable_nagle = true;               // TCP_NODELAY
    } performance;
    
    // Monitoring and Statistics
    struct MonitoringConfig {
        bool enable_statistics = true;
        int stats_update_interval_ms = 1000;    // How often to update stats
        bool enable_connection_events = true;   // Log connection/disconnection
        bool enable_message_counting = true;    // Count messages/bytes
        bool enable_latency_measurement = false; // Measure message latency
        size_t max_stats_history = 1000;        // Number of historical data points
    } monitoring;
    
    // Logging Configuration
    struct LoggingConfig {
        enum Level {
            TRACE = 0,
            DEBUG = 1,
            INFO = 2,
            WARNING = 3,
            ERROR = 4
        };
        
        Level log_level = INFO;
        bool log_to_console = true;
        bool log_to_file = false;
        std::string log_file_path;
        size_t max_log_file_size_mb = 100;
        int max_log_files = 5;                   // Log rotation
        bool enable_async_logging = true;       // Non-blocking logging
    } logging;
    
    // Network Interface Configuration
    struct NetworkInterfaceConfig {
        std::string default_interface;           // e.g., "eth0"
        bool auto_detect_interfaces = true;     // Automatically detect available interfaces
        std::vector<std::string> preferred_interfaces; // Preference order
        bool validate_interfaces_on_startup = true;
    } network_interface;
    
    // Environment Variable Overrides
    struct EnvironmentConfig {
        std::string prefix = "DELILA_NET_";      // Environment variable prefix
        bool enable_env_override = true;        // Allow env var overrides
        std::map<std::string, std::string> env_mappings = {
            {"TIMEOUT", "connection_defaults.send_timeout_ms"},
            {"HWM", "connection_defaults.send_hwm"},
            {"INTERFACE", "network_interface.default_interface"},
            {"LOG_LEVEL", "logging.log_level"},
            {"THREADS", "context.io_threads"}
        };
    } environment;
    
    // Validation function
    Result<std::monostate> validate() const;
    
    // Load configuration from JSON file
    static Result<NetworkConfig> loadFromFile(const std::string& filepath);
    
    // Load configuration from JSON string
    static Result<NetworkConfig> loadFromJson(const std::string& json_str);
    
    // Apply environment variable overrides
    void applyEnvironmentOverrides();
    
    // Get configuration for specific profile
    static Result<NetworkConfig> getProfileConfig(const std::string& profile_name);
    
    // Available profiles
    static std::vector<std::string> getAvailableProfiles() {
        return {"development", "production", "test", "benchmark"};
    }
};

// Pattern-specific configurations
struct PatternConfig {
    // PUB/SUB specific settings
    struct PubSubConfig {
        std::vector<std::string> default_topics;
        int topic_filter_cache_size = 1000;
        bool enable_topic_stats = true;
    };
    
    // PUSH/PULL specific settings
    struct PushPullConfig {
        bool enable_load_balancing = true;
        int fair_queuing_timeout_ms = 100;
    };
    
    // DEALER/ROUTER specific settings
    struct DealerRouterConfig {
        std::string default_identity;
        bool enable_routing_table = true;
        int routing_table_cleanup_interval_ms = 60000;
    };
    
    // REQ/REP specific settings
    struct ReqRepConfig {
        int request_timeout_ms = 30000;
        bool enable_request_correlation = true;
        int max_pending_requests = 100;
    };
};

}  // namespace DELILA::Net
```

#### Configuration File Formats

**JSON Configuration with Multiple Profiles** (`config/default_config.json`):
- **Development Profile**: Lower buffers, debug logging, relaxed timeouts
- **Production Profile**: Optimized buffers, info logging, standard timeouts  
- **Test Profile**: Minimal buffers, IPC transport, fast timeouts
- **Benchmark Profile**: Maximum buffers, minimal logging, performance focus

**Configuration Schema Validation** (`config/config_schema.json`):
- JSON Schema v7 validation for all configuration files
- Enforces data types, ranges, and required fields
- Validates network endpoints and interface names
- Prevents invalid configuration combinations

**Environment Variable Support** (`config/environment_variables.md`):
- Complete override system using `DELILA_NET_*` variables
- Profile selection: `DELILA_NET_PROFILE=production`  
- Buffer settings: `DELILA_NET_HIGH_WATER_MARK=4000`
- Network settings: `DELILA_NET_DEFAULT_INTERFACE=eth0`
- All variables documented with valid ranges and examples

**Environment Variable Overrides**:
```bash
# Buffer settings
export DELILA_NET_HIGH_WATER_MARK=4000
export DELILA_NET_RECEIVE_BUFFER_SIZE=2000
export DELILA_NET_BUFFER_POOL_SIZE_GB=8

# Performance settings
export DELILA_NET_COMPRESSION_ENABLED=true
export DELILA_NET_COMPRESSION_LEVEL=3
export DELILA_NET_TIMEOUT_MS=10000

# Error handling
export DELILA_NET_GAP_ALERT_THRESHOLD=20
export DELILA_NET_MAX_RETRY_ATTEMPTS=100

# Network settings
export DELILA_NET_DEFAULT_INTERFACE=eth1
export DELILA_NET_DEFAULT_TRANSPORT=tcp

# Logging
export DELILA_NET_LOG_LEVEL=debug
export DELILA_NET_LOG_CONNECTION_EVENTS=true

# Endpoint overrides
export DELILA_NET_DATA_PUBLISHER_ENDPOINT="tcp://192.168.1.100:5555"
export DELILA_NET_CONTROL_SERVER_ENDPOINT="ipc:///tmp/delila_control"
```

**Database Schema** (SQLite/PostgreSQL):
```sql
-- Configuration table
CREATE TABLE delila_net_config (
    id INTEGER PRIMARY KEY,
    config_name VARCHAR(255) UNIQUE NOT NULL,
    config_version VARCHAR(50) NOT NULL DEFAULT '1.0',
    
    -- Buffer settings
    high_water_mark INTEGER DEFAULT 2000,
    receive_buffer_size INTEGER DEFAULT 1000,
    buffer_pool_size_gb INTEGER DEFAULT 4,
    
    -- Performance settings
    compression_enabled BOOLEAN DEFAULT true,
    compression_level INTEGER DEFAULT 1,
    timeout_ms INTEGER DEFAULT 5000,
    
    -- Error handling
    gap_alert_threshold INTEGER DEFAULT 10,
    max_retry_attempts INTEGER DEFAULT -1,
    initial_retry_interval_ms INTEGER DEFAULT 100,
    max_retry_interval_ms INTEGER DEFAULT 30000,
    
    -- Network settings
    default_interface VARCHAR(255),
    default_transport VARCHAR(50) DEFAULT 'auto',
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT true
);

-- Endpoint configurations
CREATE TABLE delila_net_endpoints (
    id INTEGER PRIMARY KEY,
    config_id INTEGER REFERENCES delila_net_config(id),
    endpoint_name VARCHAR(255) NOT NULL,
    endpoint_url VARCHAR(512) NOT NULL,
    transport_type VARCHAR(50) DEFAULT 'auto',
    is_active BOOLEAN DEFAULT true
);

-- Insert default configuration
INSERT INTO delila_net_config (config_name) VALUES ('default');
INSERT INTO delila_net_endpoints (config_id, endpoint_name, endpoint_url) VALUES 
    (1, 'data_publisher', 'tcp://*:5555'),
    (1, 'control_server', 'tcp://*:5556'),
    (1, 'status_collector', 'tcp://*:5557');
```

#### Configuration Loading Priority

**Layered Configuration System** (highest priority first):
1. **Command line arguments** (runtime overrides)
2. **Environment variables** (deployment-specific)  
3. **Configuration file** (application defaults)
4. **Database settings** (shared/centralized config)
5. **Compiled defaults** (fallback values)

**Example Usage**:
```cpp
// Load with priority layering
auto config_result = NetworkConfig::loadFromFile("config.json");
if (!isOk(config_result)) {
    // Fallback to defaults
    config_result = Result<NetworkConfig>{NetworkConfig{}};
}
auto config = getValue(config_result);

// Override with environment variables
auto env_config_result = NetworkConfig::loadFromEnvironment();
if (isOk(env_config_result)) {
    config = config.merge(getValue(env_config_result));
}

// Validate final configuration
auto validation_result = config.validate();
if (!isOk(validation_result)) {
    log_error("Invalid configuration: {}", getError(validation_result).message);
    return -1;
}
```

#### Configuration Validation Rules

```cpp
Result<void> NetworkConfig::validate() const {
    // Buffer size validation
    if (high_water_mark <= 0 || high_water_mark > 100000) {
        return Error{Error::INVALID_CONFIG, "high_water_mark must be 1-100000"};
    }
    
    if (receive_buffer_size <= 0 || receive_buffer_size > high_water_mark) {
        return Error{Error::INVALID_CONFIG, "receive_buffer_size must be 1 to high_water_mark"};
    }
    
    if (buffer_pool_size < 1024*1024*100) {  // Minimum 100MB
        return Error{Error::INVALID_CONFIG, "buffer_pool_size must be at least 100MB"};
    }
    
    // Compression validation
    if (compression_level < 1 || compression_level > 12) {
        return Error{Error::INVALID_CONFIG, "compression_level must be 1-12"};
    }
    
    // Timeout validation
    if (timeout_ms < 100 || timeout_ms > 300000) {  // 100ms to 5 minutes
        return Error{Error::INVALID_CONFIG, "timeout_ms must be 100-300000"};
    }
    
    // Retry validation
    if (max_retry_attempts == 0 || max_retry_attempts < -1) {
        return Error{Error::INVALID_CONFIG, "max_retry_attempts must be -1 (infinite) or > 0"};
    }
    
    if (initial_retry_interval_ms < 10 || initial_retry_interval_ms > max_retry_interval_ms) {
        return Error{Error::INVALID_CONFIG, "invalid retry interval range"};
    }
    
    // Network interface validation
    if (!default_interface.empty()) {
        // Validate interface exists on system
        if (!isValidNetworkInterface(default_interface)) {
            return Error{Error::INVALID_CONFIG, "network interface not found: " + default_interface};
        }
    }
    
    return std::monostate{};  // Success
}
```

### 6. Monitoring & Logging

```cpp
namespace DELILA::Net {

struct ConnectionStats {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t messages_sent;
    uint64_t messages_received;
    double current_rate_mbps;
    uint64_t sequence_gaps;
    std::chrono::steady_clock::time_point last_activity;
};

}  // namespace DELILA::Net
```

## Thread Safety Design

### Overview
The library follows a "share nothing" approach for optimal performance and simplicity.

### Thread Safety Rules

1. **ZeroMQ Context**
   - One context per process
   - Shared between all threads (context is thread-safe)
   - Created at application startup

2. **Connection Objects**
   - Each thread owns its Connection instances
   - Never pass Connection objects between threads
   - Thread-local storage for connection maps

3. **Serializers**
   - Each thread creates its own BinarySerializer
   - Maintains thread-local compression contexts
   - No locking overhead

4. **Message Queues**
   - Lock-free single producer/single consumer queue
   - One receiver thread per connection (producer)
   - One processing thread (consumer)
   - Falls back to mutex if architecture doesn't support lock-free

5. **Statistics**
   - All counters use std::atomic
   - Lock-free increment/fetch operations
   - Can be read from any thread safely

### Example Thread-Safe Usage

```cpp
// Application startup (main thread)
auto context = std::make_shared<zmq::context_t>(1);

// Worker thread
void workerThread(std::shared_ptr<zmq::context_t> ctx) {
    // Thread-local connection
    Connection publisher(Connection::PUBLISHER);
    publisher.setContext(ctx);
    publisher.bind("tcp://*:5555");
    
    // Thread-local serializer
    BinarySerializer serializer;
    
    // Process data...
}

// Receiver thread
void receiverThread(std::shared_ptr<zmq::context_t> ctx, 
                   LockFreeQueue& queue) {
    Connection subscriber(Connection::SUBSCRIBER);
    subscriber.setContext(ctx);
    subscriber.connect("tcp://localhost:5555");
    
    while (running) {
        auto msg = subscriber.receiveRaw();
        if (isOk(msg)) {
            queue.push(Message{getValue(msg)});
        }
    }
}
```

## Thread Model and Synchronization

### Overview
The DELILA2 networking library follows a strict thread-local ownership model for ZeroMQ sockets with minimal synchronization. This approach ensures thread safety while maximizing performance.

### Thread Ownership Model

```cpp
namespace DELILA::Net {

/**
 * Thread Safety Rules:
 * 1. ZeroMQ Context: One per process, shared by all threads (thread-safe)
 * 2. ZeroMQ Sockets: Thread-local only, never shared between threads
 * 3. Connection Objects: Thread-local only, one per thread per endpoint
 * 4. BinarySerializer: Thread-local instances (not shared)
 * 5. Statistics: Lock-free atomic counters only
 */

class ThreadModel {
public:
    // Thread types in the system
    enum ThreadType {
        MAIN_THREAD,        // Main application thread
        RECEIVER_THREAD,    // Dedicated receiver thread per module
        SENDER_THREAD,      // Optional dedicated sender thread
        WORKER_THREAD       // General worker threads
    };
    
    // Thread-local storage structure
    struct ThreadLocalData {
        ThreadType type;
        std::string thread_name;
        std::unordered_map<std::string, std::unique_ptr<Connection>> connections;
        std::unique_ptr<BinarySerializer> serializer;
        std::chrono::steady_clock::time_point created_at;
        std::atomic<uint64_t> messages_processed{0};
    };
    
    // Get thread-local data
    static ThreadLocalData& getThreadLocalData();
    
    // Thread management functions
    static void initializeThread(ThreadType type, const std::string& name);
    static void cleanupThread();
    static ThreadType getCurrentThreadType();
    static std::string getCurrentThreadName();
};

}  // namespace DELILA::Net
```

### Lock-Free Communication Patterns

**Single Producer/Single Consumer Queue:**
```cpp
namespace DELILA::Net {

template<typename T, size_t Size = 2048>
class SPSCQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    
private:
    // Cache line aligned to prevent false sharing
    alignas(64) std::atomic<size_t> write_index_{0};
    alignas(64) std::atomic<size_t> read_index_{0};
    
    // Ring buffer for messages
    std::array<T, Size> buffer_;
    
public:
    // Non-blocking push (producer thread only)
    bool push(T&& item) {
        const size_t current_write = write_index_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) % Size;
        
        if (next_write == read_index_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        
        buffer_[current_write] = std::move(item);
        write_index_.store(next_write, std::memory_order_release);
        return true;
    }
    
    // Non-blocking pop (consumer thread only)
    bool pop(T& item) {
        const size_t current_read = read_index_.load(std::memory_order_relaxed);
        
        if (current_read == write_index_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = std::move(buffer_[current_read]);
        read_index_.store((current_read + 1) % Size, std::memory_order_release);
        return true;
    }
    
    // Check if queue is empty (approximate)
    bool empty() const {
        return read_index_.load(std::memory_order_acquire) == 
               write_index_.load(std::memory_order_acquire);
    }
    
    // Get approximate size
    size_t size() const {
        const size_t write_idx = write_index_.load(std::memory_order_acquire);
        const size_t read_idx = read_index_.load(std::memory_order_acquire);
        return (write_idx >= read_idx) ? (write_idx - read_idx) : 
                                        (Size - read_idx + write_idx);
    }
};

}  // namespace DELILA::Net
```

### Thread-Safe Statistics

```cpp
namespace DELILA::Net {

class ThreadSafeStatistics {
private:
    // Atomic counters for thread-safe updates
    mutable std::atomic<uint64_t> bytes_sent_{0};
    mutable std::atomic<uint64_t> bytes_received_{0};
    mutable std::atomic<uint64_t> messages_sent_{0};
    mutable std::atomic<uint64_t> messages_received_{0};
    mutable std::atomic<uint64_t> connection_attempts_{0};
    mutable std::atomic<uint64_t> connection_failures_{0};
    mutable std::atomic<uint64_t> reconnections_{0};
    
    // Rate calculation (approximate, updated periodically)
    mutable std::atomic<double> send_rate_mbps_{0.0};
    mutable std::atomic<double> recv_rate_mbps_{0.0};
    
    // Last update timestamp
    mutable std::atomic<std::chrono::steady_clock::time_point::rep> last_update_{0};
    
public:
    // Increment operations (thread-safe)
    void incrementBytesSent(uint64_t bytes) {
        bytes_sent_.fetch_add(bytes, std::memory_order_relaxed);
    }
    
    void incrementBytesReceived(uint64_t bytes) {
        bytes_received_.fetch_add(bytes, std::memory_order_relaxed);
    }
    
    void incrementMessagesSent() {
        messages_sent_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void incrementMessagesReceived() {
        messages_received_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Get current values (thread-safe)
    uint64_t getBytesSent() const {
        return bytes_sent_.load(std::memory_order_relaxed);
    }
    
    uint64_t getBytesReceived() const {
        return bytes_received_.load(std::memory_order_relaxed);
    }
    
    // Rate calculation (updated periodically by one thread)
    void updateRates() {
        // Implementation would calculate rates based on time intervals
        // This is called periodically by a single thread to avoid contention
    }
    
    double getSendRateMbps() const {
        return send_rate_mbps_.load(std::memory_order_relaxed);
    }
    
    double getRecvRateMbps() const {
        return recv_rate_mbps_.load(std::memory_order_relaxed);
    }
};

}  // namespace DELILA::Net
```

### Thread Lifecycle Management

```cpp
namespace DELILA::Net {

class ThreadManager {
public:
    // Thread creation with proper setup
    template<typename F>
    static std::thread createReceiverThread(const std::string& thread_name, F&& func) {
        return std::thread([thread_name = std::string(thread_name), func = std::forward<F>(func)]() {
            // Initialize thread-local data
            ThreadModel::initializeThread(ThreadModel::RECEIVER_THREAD, thread_name);
            
            // Set thread name (platform-specific)
            setThreadName(thread_name);
            
            try {
                func();
            } catch (const std::exception& e) {
                // Log error and cleanup
                logError("Thread " + thread_name + " crashed: " + e.what());
            }
            
            // Cleanup thread-local resources
            ThreadModel::cleanupThread();
        });
    }
    
    // Platform-specific thread naming
    static void setThreadName(const std::string& name) {
#ifdef __linux__
        pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#elif defined(__APPLE__)
        pthread_setname_np(name.c_str());
#endif
    }
    
    // Thread priority setting (requires privileges on some systems)
    static bool setThreadPriority(int priority) {
#ifdef __linux__
        struct sched_param param;
        param.sched_priority = priority;
        return pthread_setschedparam(pthread_self(), SCHED_OTHER, &param) == 0;
#elif defined(__APPLE__)
        // macOS uses different priority system
        thread_precedence_policy_data_t policy;
        policy.importance = priority;
        return thread_policy_set(mach_thread_self(), THREAD_PRECEDENCE_POLICY,
                                (thread_policy_t)&policy, 
                                THREAD_PRECEDENCE_POLICY_COUNT) == KERN_SUCCESS;
#endif
        return false;
    }
    
    // CPU affinity (Linux only)
    static bool setThreadAffinity(const std::vector<int>& cpu_cores) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int core : cpu_cores) {
            CPU_SET(core, &cpuset);
        }
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#endif
        return false; // Not supported on macOS
    }
};

}  // namespace DELILA::Net
```

### Synchronization Rules

**Critical Rules:**
1. **Never share ZeroMQ sockets between threads**
2. **Each thread creates its own Connection objects**
3. **Use lock-free atomic operations for statistics**
4. **Communicate between threads only via lock-free queues**
5. **Thread-local BinarySerializer instances only**

**Thread Communication Pattern:**
```
Main Thread:
├── Creates ZeroMQ context (shared)
├── Spawns receiver threads
└── Manages application logic

Receiver Thread (per module):
├── Creates thread-local connections
├── Receives messages via ZeroMQ
├── Pushes messages to lock-free queue
└── Updates atomic statistics

Worker Threads:
├── Pop messages from lock-free queue
├── Process messages using thread-local serializer
└── Send responses via thread-local connections
```

**Memory Barriers and Ordering:**
- Use `memory_order_relaxed` for counters and non-critical data
- Use `memory_order_acquire`/`memory_order_release` for queue operations
- Use `memory_order_seq_cst` only when strict ordering is required

## Platform-Specific Implementation

### Overview
The library uses compile-time platform detection with minimal runtime checks, avoiding external dependencies.

### Compile-Time Platform Detection

```cpp
// In CMakeLists.txt
if(APPLE)
    target_compile_definitions(delila_net PRIVATE DELILA_PLATFORM_MACOS)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(delila_net PRIVATE DELILA_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
```

### Byte Order Verification

```cpp
namespace DELILA::Net {

// Compile-time endianness check
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, 
              "DELILA networking library requires little-endian platform");

}  // namespace DELILA::Net
```

### Network Interface Validation

```cpp
namespace DELILA::Net {

// Platform-independent interface validation using POSIX
class NetworkInterface {
public:
    static Result<bool> isValidInterface(const std::string& interface_name) {
        struct ifaddrs* ifaddr;
        if (getifaddrs(&ifaddr) == -1) {
            return Error{Error::SYSTEM_ERROR, "Failed to get interface list"};
        }
        
        bool found = false;
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_name && interface_name == ifa->ifa_name) {
                found = true;
                break;
            }
        }
        
        freeifaddrs(ifaddr);
        return found;
    }
    
    static Result<std::vector<std::string>> listInterfaces() {
        std::vector<std::string> interfaces;
        struct ifaddrs* ifaddr;
        
        if (getifaddrs(&ifaddr) == -1) {
            return Error{Error::SYSTEM_ERROR, "Failed to get interface list"};
        }
        
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_name && ifa->ifa_addr) {
                interfaces.emplace_back(ifa->ifa_name);
            }
        }
        
        freeifaddrs(ifaddr);
        return interfaces;
    }
};

}  // namespace DELILA::Net
```

### High-Resolution Timing

```cpp
namespace DELILA::Net {

// Use std::chrono for all timing (portable and sufficient)
inline uint64_t getCurrentTimestampNs() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto ns = duration_cast<nanoseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(ns.count());
}

// For absolute timestamps (e.g., in headers)
inline uint64_t getUnixTimestampNs() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ns = duration_cast<nanoseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(ns.count());
}

}  // namespace DELILA::Net
```

### Platform-Specific Headers

```cpp
// include/delila/net/utils/Platform.hpp
#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

// POSIX headers (available on both Linux and macOS)
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>

// Platform detection macros
#if defined(DELILA_PLATFORM_LINUX)
    #include <linux/if.h>
#elif defined(DELILA_PLATFORM_MACOS)
    #include <net/if_dl.h>
#endif
```

### Build Configuration

```cmake
# Minimum versions for platform support
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # glibc 2.17+ for full C++11 support
    execute_process(
        COMMAND ldd --version
        OUTPUT_VARIABLE LDD_VERSION
    )
    # Parse and verify glibc version
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS 10.12+ for full C++11/14 support
    if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.12")
        message(WARNING "macOS 10.12+ recommended for full functionality")
    endif()
endif()
```

## ZeroMQ Integration Details

### Socket Configuration

```cpp
namespace DELILA::Net {

// ZeroMQ socket configuration constants
constexpr int DEFAULT_SEND_TIMEOUT_MS = 5000;      // 5 seconds
constexpr int DEFAULT_RECV_TIMEOUT_MS = 5000;      // 5 seconds
constexpr int DEFAULT_TCP_KEEPALIVE_IDLE = 300;    // 5 minutes
constexpr int DEFAULT_TCP_KEEPALIVE_INTVL = 30;    // 30 seconds

// Socket configuration helper
class ZMQConfig {
public:
    static void configureSocket(zmq::socket_t& socket, const NetworkConfig& config) {
        // High Water Mark (buffer limits)
        int hwm = config.high_water_mark;
        socket.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
        socket.setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
        
        // Timeouts for reliability
        int send_timeout = config.timeout_ms;
        int recv_timeout = config.timeout_ms;
        socket.setsockopt(ZMQ_SNDTIMEO, &send_timeout, sizeof(send_timeout));
        socket.setsockopt(ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
        
        // TCP Keepalive for network reliability
        int keepalive = 1;
        socket.setsockopt(ZMQ_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
        socket.setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, &DEFAULT_TCP_KEEPALIVE_IDLE, sizeof(int));
        socket.setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, &DEFAULT_TCP_KEEPALIVE_INTVL, sizeof(int));
        
        // Additional performance settings
        int linger = 0;  // Don't wait for unsent messages on close
        socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
        
        int immediate = 1;  // Queue messages only for connected peers
        socket.setsockopt(ZMQ_IMMEDIATE, &immediate, sizeof(immediate));
    }
};

}  // namespace DELILA::Net
```

### Connection String Processing

```cpp
namespace DELILA::Net {

class EndpointParser {
public:
    struct ParsedEndpoint {
        std::string protocol;  // "tcp" or "ipc"
        std::string host;      // IP address or hostname
        std::string port;      // Port number
        std::string path;      // IPC path
        bool is_local;         // True if localhost/127.0.0.1
    };
    
    static Result<ParsedEndpoint> parse(const std::string& endpoint) {
        ParsedEndpoint result;
        
        // Check for protocol prefix
        if (endpoint.find("tcp://") == 0) {
            result.protocol = "tcp";
            std::string remainder = endpoint.substr(6);  // Skip "tcp://"
            
            size_t colon_pos = remainder.find_last_of(':');
            if (colon_pos != std::string::npos) {
                result.host = remainder.substr(0, colon_pos);
                result.port = remainder.substr(colon_pos + 1);
            } else {
                return Error{Error::INVALID_CONFIG, "Invalid TCP endpoint format: " + endpoint};
            }
            
            result.is_local = (result.host == "localhost" || 
                              result.host == "127.0.0.1" ||
                              result.host == "0.0.0.0" ||
                              result.host.empty());
                              
        } else if (endpoint.find("ipc://") == 0) {
            result.protocol = "ipc";
            result.path = endpoint.substr(6);  // Skip "ipc://"
            result.is_local = true;
            
        } else {
            // No protocol specified - assume it's host:port format
            size_t colon_pos = endpoint.find_last_of(':');
            if (colon_pos != std::string::npos) {
                result.host = endpoint.substr(0, colon_pos);
                result.port = endpoint.substr(colon_pos + 1);
                result.is_local = (result.host == "localhost" || 
                                  result.host == "127.0.0.1" ||
                                  result.host.empty());
            } else {
                return Error{Error::INVALID_CONFIG, "Invalid endpoint format: " + endpoint};
            }
        }
        
        return result;
    }
    
    static std::string format(const ParsedEndpoint& ep, Connection::TransportType transport) {
        if (transport == Connection::IPC || ep.protocol == "ipc") {
            if (!ep.path.empty()) {
                return "ipc://" + ep.path;
            } else {
                // Convert TCP endpoint to IPC
                std::string name = ep.port.empty() ? "default" : ep.port;
                return "ipc:///tmp/delila_" + name;
            }
        } else {
            // TCP format
            std::string host = ep.host.empty() ? "0.0.0.0" : ep.host;
            return "tcp://" + host + ":" + ep.port;
        }
    }
};

}  // namespace DELILA::Net
```

### Usage Examples

```cpp
// Create connection with automatic transport selection
Connection data_publisher(Connection::PUBLISHER);
auto result = data_publisher.bind("localhost:5555");  // Will use IPC automatically

// Explicit TCP connection
Connection data_subscriber(Connection::SUBSCRIBER);
auto result2 = data_subscriber.connect("tcp://192.168.1.100:5555", Connection::TCP);

// IPC connection
Connection control_client(Connection::CLIENT);
auto result3 = control_client.connect("ipc:///tmp/delila_control", Connection::IPC);
```

## Logging Framework

### Overview
The library uses a callback-based logging system for maximum flexibility without dependencies.

### Logging System Design

```cpp
namespace DELILA::Net {

enum class LogLevel {
    TRACE = 0,    // Detailed function entry/exit, loop iterations
    DEBUG = 1,    // Debugging information, variable values
    INFO = 2,     // General information, connection events
    WARNING = 3,  // Warning conditions, recoverable errors
    ERROR = 4     // Error conditions, unrecoverable failures
};

// Logging callback type - application implements this
using LogCallback = std::function<void(LogLevel level, 
                                      const std::string& component,
                                      const std::string& message)>;

class Logger {
private:
    static inline LogCallback callback_ = nullptr;
    static inline LogLevel min_level_ = LogLevel::INFO;
    static inline std::atomic<bool> enabled_{true};
    
public:
    // Configuration
    static void setCallback(LogCallback callback) {
        callback_ = std::move(callback);
    }
    
    static void setMinLevel(LogLevel level) {
        min_level_ = level;
    }
    
    static void setEnabled(bool enabled) {
        enabled_ = enabled;
    }
    
    // Core logging function
    template<typename... Args>
    static void log(LogLevel level, const std::string& component, 
                   const char* format, Args&&... args) {
        if (!enabled_ || !callback_ || level < min_level_) {
            return;
        }
        
        // Format message using printf-style formatting
        char buffer[2048];
        snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
        
        callback_(level, component, std::string(buffer));
    }
};

}  // namespace DELILA::Net
```

### Logging Macros

```cpp
// Components for structured logging
#define DELILA_NET_COMPONENT_SERIALIZER   "Serializer"
#define DELILA_NET_COMPONENT_CONNECTION   "Connection"
#define DELILA_NET_COMPONENT_TRANSPORT    "Transport"
#define DELILA_NET_COMPONENT_CONFIG       "Config"
#define DELILA_NET_COMPONENT_SEQUENCE     "SequenceTracker"

// Performance-optimized macros (DEBUG/TRACE compiled out in Release)
#ifdef NDEBUG
    #define DELILA_NET_LOG_TRACE(component, ...) ((void)0)
    #define DELILA_NET_LOG_DEBUG(component, ...) ((void)0)
#else
    #define DELILA_NET_LOG_TRACE(component, ...) \
        DELILA::Net::Logger::log(DELILA::Net::LogLevel::TRACE, component, __VA_ARGS__)
    #define DELILA_NET_LOG_DEBUG(component, ...) \
        DELILA::Net::Logger::log(DELILA::Net::LogLevel::DEBUG, component, __VA_ARGS__)
#endif

// Always-available macros
#define DELILA_NET_LOG_INFO(component, ...) \
    DELILA::Net::Logger::log(DELILA::Net::LogLevel::INFO, component, __VA_ARGS__)
#define DELILA_NET_LOG_WARNING(component, ...) \
    DELILA::Net::Logger::log(DELILA::Net::LogLevel::WARNING, component, __VA_ARGS__)
#define DELILA_NET_LOG_ERROR(component, ...) \
    DELILA::Net::Logger::log(DELILA::Net::LogLevel::ERROR, component, __VA_ARGS__)
```

### Usage Examples

```cpp
// Application setup
DELILA::Net::Logger::setCallback([](DELILA::Net::LogLevel level, 
                                   const std::string& component,
                                   const std::string& message) {
    const char* level_str = "INFO";
    switch (level) {
        case DELILA::Net::LogLevel::TRACE:   level_str = "TRACE"; break;
        case DELILA::Net::LogLevel::DEBUG:   level_str = "DEBUG"; break;
        case DELILA::Net::LogLevel::INFO:    level_str = "INFO"; break;
        case DELILA::Net::LogLevel::WARNING: level_str = "WARN"; break;
        case DELILA::Net::LogLevel::ERROR:   level_str = "ERROR"; break;
    }
    
    printf("[%s] %s: %s\n", level_str, component.c_str(), message.c_str());
});

DELILA::Net::Logger::setMinLevel(DELILA::Net::LogLevel::DEBUG);

// Library usage
DELILA_NET_LOG_INFO(DELILA_NET_COMPONENT_CONNECTION, 
                   "Connected to endpoint: %s", endpoint.c_str());

DELILA_NET_LOG_DEBUG(DELILA_NET_COMPONENT_SERIALIZER, 
                    "Serialized %zu events (%zu bytes)", 
                    events.size(), serialized_size);

DELILA_NET_LOG_ERROR(DELILA_NET_COMPONENT_TRANSPORT, 
                    "Connection lost: %s (error: %d)", 
                    endpoint.c_str(), error_code);
```

### Logged Events

#### Connection Events
```cpp
// Connection establishment
DELILA_NET_LOG_INFO(DELILA_NET_COMPONENT_CONNECTION,
                   "Binding to %s (transport: %s)", 
                   endpoint.c_str(), transport_type.c_str());

// Connection loss
DELILA_NET_LOG_WARNING(DELILA_NET_COMPONENT_CONNECTION,
                      "Connection lost to %s, attempting reconnect", 
                      endpoint.c_str());
```

#### Data Transfer Statistics
```cpp
// Throughput monitoring
DELILA_NET_LOG_INFO(DELILA_NET_COMPONENT_TRANSPORT,
                   "Data rate: %.2f MB/s (%lu messages/s)", 
                   mbps, messages_per_second);

// Buffer status
DELILA_NET_LOG_DEBUG(DELILA_NET_COMPONENT_TRANSPORT,
                    "Buffer usage: %zu/%zu (%.1f%% full)", 
                    used_slots, total_slots, usage_percent);
```

#### Error Conditions
```cpp
// Sequence gaps
DELILA_NET_LOG_WARNING(DELILA_NET_COMPONENT_SEQUENCE,
                      "Sequence gap detected: expected %lu, got %lu (gap size: %lu)", 
                      expected, received, gap_size);

// Serialization errors
DELILA_NET_LOG_ERROR(DELILA_NET_COMPONENT_SERIALIZER,
                    "Checksum mismatch: expected 0x%08x, got 0x%08x", 
                    expected_checksum, actual_checksum);
```

#### Performance Metrics
```cpp
// Compression efficiency
DELILA_NET_LOG_DEBUG(DELILA_NET_COMPONENT_SERIALIZER,
                    "Compression: %zu -> %zu bytes (%.1f%% reduction)", 
                    uncompressed_size, compressed_size, reduction_percent);

// Network interface selection
DELILA_NET_LOG_INFO(DELILA_NET_COMPONENT_TRANSPORT,
                   "Selected transport: %s for endpoint %s", 
                   transport_name, endpoint.c_str());
```

### Integration with Application Logging

The callback approach allows seamless integration with any logging framework:

```cpp
// Integration with spdlog
auto logger = spdlog::get("delila_net");
DELILA::Net::Logger::setCallback([logger](DELILA::Net::LogLevel level, 
                                         const std::string& component,
                                         const std::string& message) {
    switch (level) {
        case DELILA::Net::LogLevel::TRACE:
            logger->trace("[{}] {}", component, message);
            break;
        case DELILA::Net::LogLevel::DEBUG:
            logger->debug("[{}] {}", component, message);
            break;
        case DELILA::Net::LogLevel::INFO:
            logger->info("[{}] {}", component, message);
            break;
        case DELILA::Net::LogLevel::WARNING:
            logger->warn("[{}] {}", component, message);
            break;
        case DELILA::Net::LogLevel::ERROR:
            logger->error("[{}] {}", component, message);
            break;
    }
});
```

## Build System Configuration

### CMake Configuration

```cmake
# CMakeLists.txt for DELILA Network Library
cmake_minimum_required(VERSION 3.15)
project(delila_net VERSION 1.0.0 LANGUAGES CXX)

# C++17 requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    add_compile_options(-Wall -Wextra -Werror -pedantic)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Platform detection
if(APPLE)
    add_definitions(-DDELILA_PLATFORM_MACOS)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DDELILA_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Dependency management: find_package with FetchContent fallback
find_package(PkgConfig REQUIRED)

# ZeroMQ dependency
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ QUIET libzmq>=4.3.0)
if(NOT ZMQ_FOUND)
    message(STATUS "System ZeroMQ not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        zeromq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG v4.3.4
    )
    FetchContent_MakeAvailable(zeromq)
    set(ZMQ_LIBRARIES zmq)
endif()

# LZ4 dependency
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 QUIET liblz4>=1.9.0)
if(NOT LZ4_FOUND)
    message(STATUS "System LZ4 not found, using FetchContent")
    FetchContent_Declare(
        lz4
        GIT_REPOSITORY https://github.com/lz4/lz4.git
        GIT_TAG v1.9.4
        SOURCE_SUBDIR build/cmake
    )
    FetchContent_MakeAvailable(lz4)
    set(LZ4_LIBRARIES lz4_static)
endif()

# xxHash dependency
find_path(XXHASH_INCLUDE_DIR xxhash.h)
find_library(XXHASH_LIBRARY xxhash)
if(NOT XXHASH_INCLUDE_DIR OR NOT XXHASH_LIBRARY)
    message(STATUS "System xxHash not found, using FetchContent")
    FetchContent_Declare(
        xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v0.8.1
        SOURCE_SUBDIR cmake_unofficial
    )
    FetchContent_MakeAvailable(xxhash)
    set(XXHASH_LIBRARIES xxhash)
endif()

# Protocol Buffers dependency
find_package(Protobuf 3.0 REQUIRED)
if(NOT Protobuf_FOUND)
    message(STATUS "System Protobuf not found, using FetchContent")
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG v3.21.12
        SOURCE_SUBDIR cmake
    )
    FetchContent_MakeAvailable(protobuf)
endif()

# Generate protobuf sources
file(GLOB PROTO_FILES proto/*.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Library source files
set(DELILA_NET_SOURCES
    src/serialization/BinarySerializer.cpp
    src/transport/Connection.cpp
    src/transport/NetworkModule.cpp
    src/config/NetworkConfig.cpp
    src/utils/SequenceTracker.cpp
    src/utils/Error.cpp
    src/utils/Platform.cpp
    ${PROTO_SRCS}
)

# Library creation - both static and shared
if(BUILD_STATIC_LIBS)
    add_library(delila_net_static STATIC ${DELILA_NET_SOURCES})
    set_target_properties(delila_net_static PROPERTIES OUTPUT_NAME delila_net)
    target_include_directories(delila_net_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(delila_net_static PUBLIC
        ${ZMQ_LIBRARIES}
        ${LZ4_LIBRARIES} 
        ${XXHASH_LIBRARIES}
        ${Protobuf_LIBRARIES}
        pthread
    )
endif()

if(BUILD_SHARED_LIBS)
    add_library(delila_net_shared SHARED ${DELILA_NET_SOURCES})
    set_target_properties(delila_net_shared PROPERTIES OUTPUT_NAME delila_net)
    target_include_directories(delila_net_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(delila_net_shared PUBLIC
        ${ZMQ_LIBRARIES}
        ${LZ4_LIBRARIES}
        ${XXHASH_LIBRARIES}
        ${Protobuf_LIBRARIES}
        pthread
    )
endif()

# Installation configuration
include(GNUInstallDirs)

if(BUILD_STATIC_LIBS)
    install(TARGETS delila_net_static
        EXPORT delila_net_targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

if(BUILD_SHARED_LIBS)
    install(TARGETS delila_net_shared
        EXPORT delila_net_targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install headers
install(DIRECTORY include/delila
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Install generated protobuf headers
install(FILES ${PROTO_HDRS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/delila/net/proto
)

# Generate and install pkg-config file
configure_file(delila_net.pc.in delila_net.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/delila_net.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Export targets for find_package support
install(EXPORT delila_net_targets
    FILE delila_net_targets.cmake
    NAMESPACE delila::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/delila_net
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

### pkg-config Template (delila_net.pc.in)

```
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: delila_net
Description: DELILA2 High-Performance Networking Library
Version: @PROJECT_VERSION@
URL: https://github.com/delila/delila_net
Requires: libzmq >= 4.3.0, liblz4 >= 1.9.0, protobuf >= 3.0
Libs: -L${libdir} -ldelila_net -lxxhash -lpthread
Cflags: -I${includedir} -std=c++17
```

### Compiler Requirements

```cmake
# Minimum compiler versions
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0")
    message(FATAL_ERROR "GCC 7+ required for C++17 support")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
    message(FATAL_ERROR "Clang 5+ required for C++17 support")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
    message(FATAL_ERROR "AppleClang 10+ required for C++17 support")
endif()
```

## Implementation Phases

### Phase 1: Serialization (Current Focus)
1. Define header structures
2. Implement encode/decode functions
3. Add LZ4 compression
4. Create unit tests
5. Benchmark performance

### Phase 2: Core Messaging
1. ZeroMQ wrapper classes
2. Connection management
3. Basic message send/receive
4. Error handling

### Phase 3: Advanced Features
1. Priority queuing
2. Monitoring/statistics
3. Interface binding
4. Performance optimization

### Phase 4: Testing & Integration
1. Integration tests
2. Performance benchmarks
3. Example applications
4. Documentation

## Performance Considerations

### Memory Management
- Simple memory allocation per message (no pre-allocated pools)
- Memory pooling handled at module level, not in network library
- Zero-copy where possible using ZeroMQ features
- Direct serialization using memcpy for binary data
- **Recommended buffer sizes for 500 MB/s with 100KB-1MB messages:**
  - ZeroMQ High Water Mark: 2,000 messages (~1GB)
  - Receive buffer (std::deque): 1,000 messages (~500MB)
- **On resource exhaustion**: Return error immediately (no blocking)

### Threading Model
- **ZeroMQ Context**: One per process, shared safely between threads
- **Connection Objects**: Thread-local only, never shared
- **Receive Pattern**: Dedicated receiver thread with lock-free queue
- **Serializers**: Thread-local instances for best performance
- **Statistics**: Lock-free atomic counters
- **CPU Affinity**: Optional but not required
- **General Rule**: "Don't share stateful objects between threads"

### Network Optimization
- Large message optimization (100KB-1MB per message)
- Nagel's algorithm disabled for low latency
- Jumbo frames support for high throughput
- Consider 10GbE network for 500 MB/s sustained rate

## Testing Strategy

### Test Organization Structure
```
tests/
├── unit/                           # Unit tests
│   ├── serialization/
│   │   ├── test_binary_serializer.cpp
│   │   ├── test_compression.cpp
│   │   └── test_eventdata_format.cpp
│   ├── transport/
│   │   ├── test_connection.cpp
│   │   ├── test_transport_selection.cpp
│   │   └── test_error_handling.cpp
│   ├── config/
│   │   ├── test_network_config.cpp
│   │   └── test_config_loading.cpp
│   └── utils/
│       ├── test_sequence_tracker.cpp
│       └── test_result_pattern.cpp
├── integration/                    # Integration tests
│   ├── test_pub_sub_pattern.cpp
│   ├── test_push_pull_pattern.cpp
│   ├── test_dealer_router_pattern.cpp
│   ├── test_failure_recovery.cpp
│   └── test_multi_module.cpp
├── benchmarks/                     # Performance tests
│   ├── bench_serialization.cpp
│   ├── bench_compression.cpp
│   ├── bench_network_throughput.cpp
│   └── bench_end_to_end.cpp
├── data/                          # Test data files
│   ├── sample_events.root
│   ├── test_config.json
│   └── performance_baselines.json
└── CMakeLists.txt                 # Test build configuration
```

### Unit Tests (GoogleTest)

#### Serialization Tests
```cpp
namespace DELILA::Net::Test {

class BinarySerializerTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    BinarySerializer serializer_;
    TestDataGenerator generator_;
};

// Test cases:
TEST_F(BinarySerializerTest, SerializeEmptyEvent);
TEST_F(BinarySerializerTest, SerializeEventWithWaveform);
TEST_F(BinarySerializerTest, SerializeLargeEventBatch);
TEST_F(BinarySerializerTest, RoundTripConsistency);
TEST_F(BinarySerializerTest, CompressionEfficiency);
TEST_F(BinarySerializerTest, HeaderValidation);
TEST_F(BinarySerializerTest, SequenceNumberIncrement);
TEST_F(BinarySerializerTest, LittleEndianByteOrder);
TEST_F(BinarySerializerTest, ErrorHandlingInvalidData);

}  // namespace DELILA::Net::Test
```

#### Transport Tests
```cpp
namespace DELILA::Net::Test {

class ConnectionTest : public ::testing::Test {
protected:
    std::unique_ptr<zmq::context_t> context_;
    NetworkConfig config_;
};

// Test cases:
TEST_F(ConnectionTest, AutoTransportSelection);    // IPC for localhost, TCP for remote
TEST_F(ConnectionTest, ManualTransportOverride);   // Explicit TCP/IPC selection
TEST_F(ConnectionTest, ConnectionRecovery);        // Automatic reconnection
TEST_F(ConnectionTest, HighWaterMarkBehavior);     // Buffer overflow handling
TEST_F(ConnectionTest, TimeoutHandling);           // Send/receive timeouts
TEST_F(ConnectionTest, InterfaceBinding);          // Bind to specific interface
TEST_F(ConnectionTest, MessagePatternCorrectness); // PUB/SUB, PUSH/PULL, etc.

}  // namespace DELILA::Net::Test
```

### Integration Tests

#### Multi-Module Communication
```cpp
namespace DELILA::Net::Test {

class MultiModuleTest : public ::testing::Test {
protected:
    // Setup multiple modules in separate threads
    void SetUpModules();
    void TearDownModules();
    
    std::vector<std::thread> module_threads_;
    std::unique_ptr<zmq::context_t> shared_context_;
};

// Test scenarios:
TEST_F(MultiModuleTest, DataFetcherToMergerFlow);     // PUSH/PULL pattern
TEST_F(MultiModuleTest, MergerToSinkBroadcast);       // PUB/SUB pattern  
TEST_F(MultiModuleTest, ControllerModuleCoordination); // DEALER/ROUTER pattern
TEST_F(MultiModuleTest, FailureAndRecovery);          // Module crash scenarios
TEST_F(MultiModuleTest, LoadBalancing);               // Multiple workers
TEST_F(MultiModuleTest, BackPressureHandling);        // Buffer full scenarios

}  // namespace DELILA::Net::Test
```

### Performance Benchmarks (Google Benchmark)

#### Serialization Benchmarks
```cpp
namespace DELILA::Net::Benchmark {

class SerializationBenchmark : public ::benchmark::Fixture {
protected:
    BinarySerializer serializer_;
    std::vector<DELILA::Digitizer::EventData> test_events_;
    
public:
    void SetUp(const ::benchmark::State& state) override;
};

// Benchmark cases:
BENCHMARK_F(SerializationBenchmark, EncodeSmallEvents);      // 0KB waveforms
BENCHMARK_F(SerializationBenchmark, EncodeMediumEvents);     // 10KB waveforms  
BENCHMARK_F(SerializationBenchmark, EncodeLargeEvents);      // 100KB waveforms
BENCHMARK_F(SerializationBenchmark, BatchEncoding);         // 1000 events
BENCHMARK_F(SerializationBenchmark, CompressionOverhead);   // LZ4 vs uncompressed
BENCHMARK_F(SerializationBenchmark, MemoryUsage);           // Peak memory tracking

}  // namespace DELILA::Net::Benchmark
```

#### Network Throughput Benchmarks
```cpp
// Target performance goals:
BENCHMARK(NetworkThroughput, TCP_100MBps);      // 100 MB/s sustained
BENCHMARK(NetworkThroughput, IPC_500MBps);      // 500 MB/s peak (single machine)
BENCHMARK(NetworkThroughput, Latency_P99);      // 99th percentile latency
BENCHMARK(NetworkThroughput, CPU_Usage);        // CPU utilization at max throughput
```

### Test Data Generation

```cpp
namespace DELILA::Net::Test {

class TestDataGenerator {
private:
    std::mt19937 rng_;
    std::uniform_int_distribution<uint16_t> energy_dist_;
    std::uniform_int_distribution<uint8_t> module_dist_;
    std::uniform_int_distribution<size_t> waveform_size_dist_;
    std::uniform_real_distribution<double> frequency_dist_;  // For sine wave generation
    
public:
    // Constructor with configurable parameters
    explicit TestDataGenerator(uint32_t seed = std::random_device{}());
    
    // Generate EventData with random fields and sine wave waveforms
    DELILA::Digitizer::EventData generateSingleEvent(
        size_t waveform_size = 0,               // 0 = random size
        double frequency_mhz = 0.0              // 0 = random frequency for sine wave
    );
    
    // Generate event batches with random data
    std::vector<DELILA::Digitizer::EventData> generateEventBatch(
        size_t count,
        size_t min_waveform_size = 0,
        size_t max_waveform_size = 10000,
        bool sequential_timestamps = true       // Sequential timestamp progression
    );
    
    // Generate events for performance testing (10 MB/s target)
    std::vector<DELILA::Digitizer::EventData> generatePerformanceTestData(
        size_t total_size_mb = 10,              // 10 MB target for test validation
        size_t avg_event_size_kb = 100          // Average event size
    );
    
    // Load from ROOT files or other experimental data formats
    Result<std::vector<DELILA::Digitizer::EventData>> loadFromFile(
        const std::string& filename,
        size_t max_events = 0                   // 0 = all events
    );
    
    // Save/load test datasets for reproducible benchmarks
    Result<void> saveTestDataset(const std::string& filename, 
                                const std::vector<DELILA::Digitizer::EventData>& events);
    Result<std::vector<DELILA::Digitizer::EventData>> loadTestDataset(const std::string& filename);
    
    // Configuration for random generation
    void setSeed(uint32_t seed);
    void setEnergyRange(uint16_t min, uint16_t max);
    void setModuleRange(uint8_t min, uint8_t max);
    void setWaveformSizeRange(size_t min, size_t max);
    void setTimestampIncrement(double increment_ns);
    void setFrequencyRange(double min_mhz, double max_mhz);  // For sine wave generation

private:
    // Generate sine wave waveform data
    void generateSineWaveWaveform(DELILA::Digitizer::EventData& event, 
                                 size_t waveform_size, 
                                 double frequency_mhz,
                                 double sampling_rate_mhz = 1000.0);
};

}  // namespace DELILA::Net::Test
```

### Mock Objects for Testing

```cpp
namespace DELILA::Net::Test {

// Mock ZeroMQ socket for unit testing
class MockZMQSocket {
public:
    MOCK_METHOD(int, bind, (const std::string& endpoint));
    MOCK_METHOD(int, connect, (const std::string& endpoint));
    MOCK_METHOD(bool, send, (const std::vector<uint8_t>& data));
    MOCK_METHOD(std::vector<uint8_t>, receive, ());
    MOCK_METHOD(void, setsockopt, (int option, const void* optval, size_t optvallen));
    MOCK_METHOD(void, close, ());
};

// Mock network interface for connection testing
class MockNetworkInterface {
public:
    MOCK_METHOD(bool, isValidInterface, (const std::string& interface_name));
    MOCK_METHOD(std::vector<std::string>, listInterfaces, ());
    MOCK_METHOD(std::string, getInterfaceIP, (const std::string& interface_name));
};

// Mock EventData generator for serialization testing
class MockEventDataGenerator {
public:
    MOCK_METHOD(DELILA::Digitizer::EventData, generateEvent, (size_t waveform_size));
    MOCK_METHOD(std::vector<DELILA::Digitizer::EventData>, generateBatch, (size_t count));
};

}  // namespace DELILA::Net::Test
```

### Test Organization & Execution

#### CMake Test Configuration
```cmake
# tests/CMakeLists.txt
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Unit tests - separate executable
add_executable(delila_net_unit_tests
    unit/serialization/test_binary_serializer.cpp
    unit/transport/test_connection.cpp
    unit/config/test_network_config.cpp
    unit/utils/test_sequence_tracker.cpp
    unit/utils/test_error_handling.cpp
    common/mock_objects.cpp
    common/test_data_generator.cpp
)

target_link_libraries(delila_net_unit_tests
    delila_net
    GTest::gtest_main
    GTest::gmock
)

# Integration tests - separate executable
add_executable(delila_net_integration_tests
    integration/test_multi_module.cpp
    integration/test_pub_sub_pattern.cpp
    integration/test_push_pull_pattern.cpp
    integration/test_failure_recovery.cpp
    common/test_data_generator.cpp
)

target_link_libraries(delila_net_integration_tests
    delila_net
    GTest::gtest_main
)

# Benchmarks - separate executable  
add_executable(delila_net_benchmarks
    benchmarks/bench_serialization.cpp
    benchmarks/bench_network_throughput.cpp
    benchmarks/bench_end_to_end.cpp
    common/test_data_generator.cpp
)

target_link_libraries(delila_net_benchmarks
    delila_net
    benchmark::benchmark
)

# Test targets - sequential execution only
enable_testing()

# Unit tests
add_test(NAME UnitTests COMMAND delila_net_unit_tests)
set_tests_properties(UnitTests PROPERTIES
    TIMEOUT 300
    LABELS "unit"
)

# Integration tests  
add_test(NAME IntegrationTests COMMAND delila_net_integration_tests)
set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 600
    LABELS "integration"
    DEPENDS UnitTests  # Run after unit tests
)

# Benchmarks with 10 MB/s performance target
add_test(NAME Benchmarks COMMAND delila_net_benchmarks 
    --benchmark_min_time=1
    --benchmark_filter=".*Throughput.*"
)
set_tests_properties(Benchmarks PROPERTIES
    TIMEOUT 1200
    LABELS "benchmark"
    DEPENDS IntegrationTests  # Run after integration tests
)

# Custom test target for sequential execution
add_custom_target(test_sequential
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure
    DEPENDS delila_net_unit_tests delila_net_integration_tests delila_net_benchmarks
    COMMENT "Running all tests sequentially"
)
```

### Performance Validation

#### Acceptance Criteria
```cpp
// Performance thresholds (fail tests if not met)
constexpr double MIN_SERIALIZATION_MBPS = 500.0;      // Serialization speed
constexpr double MIN_NETWORK_THROUGHPUT_MBPS = 100.0; // Network throughput
constexpr double MAX_CPU_USAGE_PERCENT = 80.0;        // CPU usage at max load
constexpr double MAX_MEMORY_USAGE_GB = 4.0;           // Memory usage limit
constexpr uint64_t MAX_LATENCY_P99_MS = 10;           // 99th percentile latency
```

## API Examples

### Sending Binary Data
```cpp
using namespace DELILA::Net;

// Create connection with automatic transport selection
Connection data_publisher(Connection::PUBLISHER);
// Single machine - will use IPC automatically for better performance
data_publisher.bind("127.0.0.1:5555", Connection::AUTO);
// Multi-machine - explicitly use TCP
// data_publisher.bind("192.168.1.100:5555", Connection::TCP);

// Create serializer instance
BinarySerializer serializer;
serializer.setCompression(true, 1);  // Enable LZ4 with fast compression

// Create and send event data
std::vector<EventData> events;
// ... fill events ...

auto result = serializer.encode(events);
if (isOk(result)) {
    // Use low-level API for maximum performance with binary data
    auto status = data_publisher.sendRaw(getValue(result));
    if (!isOk(status)) {
        // Handle send error
        const auto& error = getError(status);
        log_error("Send failed: {}", error.message);
    }
} else {
    // Handle encoding error
    const auto& error = getError(result);
    log_error("Encoding failed: {}", error.message);
}
```

### Receiving Binary Data
```cpp
using namespace DELILA::Net;

// Create connection with automatic transport selection
Connection data_subscriber(Connection::SUBSCRIBER);
data_subscriber.connect("127.0.0.1:5555", Connection::AUTO);  // Will use IPC for localhost

// Create serializer instance (same config as sender)
BinarySerializer serializer;

// Receive raw binary data for maximum performance
auto raw_result = data_subscriber.receiveRaw();
if (!isOk(raw_result)) {
    log_error("Receive failed: {}", getError(raw_result).message);
    return;
}

const auto& raw_data = getValue(raw_result);

// Decode events
auto decode_result = serializer.decode(raw_data);
if (!isOk(decode_result)) {
    log_error("Decode failed: {}", getError(decode_result).message);
    return;
}

const auto& events = getValue(decode_result);

// Check sequence for gaps
const auto* header = reinterpret_cast<const BinaryDataHeader*>(raw_data.data());
auto seq_result = tracker.checkSequence(header->sequence_number);
if (isOk(seq_result)) {
    // Process events - no gap
    processEvents(events);
} else {
    // Log gap and continue
    log_warning("Sequence gap detected: {}", getError(seq_result).message);
    processEvents(events);  // Still process the data
}
```

### Control Messages
```cpp
using namespace DELILA::Net;

// Send state change
StateChangeRequest request;
request.set_new_state(State::RUNNING);
control_client.send(request);

// Receive response
auto response = control_client.receive<StateChangeResponse>();
```

## Dependencies

- ZeroMQ (libzmq) >= 4.3.0
- LZ4 >= 1.9.0
- xxHash >= 0.8.0
- Protocol Buffers >= 3.0
- C++17 or later
- GoogleTest (for testing)
- ROOT >= 6.0 (optional, for test data)
- EventData.hpp (common DAQ data structure)

## Build System

- CMake >= 3.15
- Support for pkg-config and system packages
- Separate headers for each component
- GoogleTest for unit and integration testing

## Directory Structure

```
lib/net/
├── include/delila/net/           # Public headers
│   ├── delila_net.hpp           # Main include (includes all components)
│   ├── serialization/           # Binary and message serialization
│   ├── transport/               # ZeroMQ connections and networking
│   ├── config/                  # Configuration management
│   ├── utils/                   # Error handling, sequence tracking
│   └── proto/                   # Protocol buffer generated headers
├── src/                         # Implementation files
│   ├── serialization/           # BinarySerializer implementation
│   ├── transport/               # Connection, NetworkModule implementation
│   ├── config/                  # NetworkConfig implementation
│   └── utils/                   # Error, SequenceTracker implementation
├── proto/                       # Protocol buffer definitions
│   └── control_messages.proto   # State, heartbeat, status, error messages
├── tests/                       # Test files
│   ├── unit/                    # Unit tests (separated by component)
│   │   ├── serialization/       # BinarySerializer tests
│   │   ├── transport/           # Connection, NetworkModule tests
│   │   ├── config/              # NetworkConfig tests
│   │   └── utils/               # Error, SequenceTracker tests
│   ├── integration/             # Multi-component integration tests
│   └── benchmarks/              # Performance benchmarks
├── examples/                    # Example applications
│   ├── data_flow/               # DAQ data flow patterns
│   │   ├── data_fetcher_example.cpp    # PUSH data to merger
│   │   ├── merger_example.cpp          # PULL/PUB timestamp sorting
│   │   └── recorder_example.cpp       # SUB data from merger  
│   ├── control_patterns/        # Control and coordination
│   │   ├── controller_example.cpp      # DEALER/ROUTER coordination
│   │   └── module_example.cpp          # Module with control interface
│   └── performance/             # Performance testing tools
├── config/                     # Configuration files
│   ├── default_config.json     # Multi-profile configuration
│   ├── config_schema.json      # JSON schema validation
│   └── environment_variables.md # Environment variable documentation
├── CMakeLists.txt              # Main build configuration
└── delila_net.pc.in           # pkg-config template
```

## Future Enhancements

1. **Encryption Support**: Optional TLS/encryption layer
2. **Compression Options**: Support for Zstd, Snappy
3. **Protocol Evolution**: Versioned protocol support
4. **Cloud Integration**: S3/object storage backends
5. **Metrics Export**: Prometheus/OpenTelemetry integration