#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <sstream>
#include <queue>
#include <mutex>
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include "Serializer.hpp"

namespace DELILA::Net {

// Simple configuration following KISS principle
struct TransportConfig {
    std::string data_address = "tcp://localhost:5555";
    std::string status_address = "tcp://localhost:5556";  
    std::string command_address = "tcp://localhost:5557";
    bool bind_data = true;
    bool bind_status = true;
    bool bind_command = false;
    
    // Pattern specification for data channel (Phase 2: PUSH/PULL support)
    std::string data_pattern = "PUB";  // "PUB", "SUB", "PUSH", "PULL"
    
    // Role specification for PUB/SUB pattern (backward compatibility)
    bool is_publisher = true;  // true = PUB (send), false = SUB (receive)
    
    // Server mode configuration for REP pattern support
    bool server_mode = false;
    bool status_server = false;
    bool command_server = false;
};

// Component status for health monitoring
struct ComponentStatus {
    std::string component_id;
    std::string state;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, double> metrics;
    std::string error_message;
    uint64_t heartbeat_counter = 0;
};

// Command structures for state machine control
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
    std::string command_id;
    CommandType type;
    std::string target_component;
    std::map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point timestamp;
};

struct CommandResponse {
    std::string command_id;
    bool success = false;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> result_data;
};

// KISS: Simple, focused interface
class ZMQTransport {
public:
    ZMQTransport();
    virtual ~ZMQTransport();
    
    // Connection management
    bool Configure(const TransportConfig& config);
    bool ConfigureFromJSON(const nlohmann::json& config);
    bool ConfigureFromFile(const std::string& filename);
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    
    // Data functions (using existing Serializer)
    bool Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events);
    std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> Receive();
    
    // Status functions  
    bool SendStatus(const ComponentStatus& status);
    std::unique_ptr<ComponentStatus> ReceiveStatus();
    
    // Command functions
    CommandResponse SendCommand(const Command& command);
    std::unique_ptr<Command> ReceiveCommand();
    
    // Server-side methods for REP pattern
    bool StartServer();
    void StopServer();
    bool IsServerRunning() const;
    
    // Command handling (REP side) 
    std::unique_ptr<Command> WaitForCommand(int timeout_ms = 1000);
    bool SendCommandResponse(const CommandResponse& response);
    
    // Status handling (REP side)
    bool WaitForStatusRequest(int timeout_ms = 1000);
    bool SendStatusResponse(const ComponentStatus& status);
    
    // Serialization control functions
    void EnableCompression(bool enable);
    void EnableChecksum(bool enable);
    bool IsCompressionEnabled() const;
    bool IsChecksumEnabled() const;
    
    // Zero-copy optimization functions
    void EnableZeroCopy(bool enable);
    bool IsZeroCopyEnabled() const;
    
    // Memory pool optimization functions
    void EnableMemoryPool(bool enable);
    bool IsMemoryPoolEnabled() const;
    void SetMemoryPoolSize(size_t size);
    size_t GetMemoryPoolSize() const;
    size_t GetPooledContainerCount() const;

private:
    bool fConnected = false;
    bool fConfigured = false;
    TransportConfig fConfig;
    uint64_t fSequenceNumber = 0;
    
    // Serialization settings
    bool fCompressionEnabled = true;   // Default enabled
    bool fChecksumEnabled = true;      // Default enabled
    bool fZeroCopyEnabled = false;     // Default disabled for safety
    std::unique_ptr<Serializer> fSerializer;  // Persistent serializer
    
    // Memory pool settings
    bool fMemoryPoolEnabled = true;    // Default enabled for performance
    size_t fMemoryPoolSize = 20;       // Default pool size
    std::queue<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>> fEventContainerPool;
    mutable std::mutex fPoolMutex;     // Thread safety for pool access
    
    // ZeroMQ context and sockets (following KISS - start with data only)
    std::unique_ptr<zmq::context_t> fContext;
    std::unique_ptr<zmq::socket_t> fDataSocket;  // PUB socket for sending
    std::unique_ptr<zmq::socket_t> fReceiveSocket;  // SUB socket for receiving
    std::unique_ptr<zmq::socket_t> fStatusSocket;  // For status communication
    std::unique_ptr<zmq::socket_t> fCommandSocket;  // For command communication
    
    // Server mode sockets (REP pattern)
    std::unique_ptr<zmq::socket_t> fStatusServerSocket;  // REP socket for status server
    std::unique_ptr<zmq::socket_t> fCommandServerSocket; // REP socket for command server
    bool fServerRunning = false;
    
    // Helper methods for simple JSON serialization (KISS approach)
    std::string SerializeStatus(const ComponentStatus& status) const;
    std::unique_ptr<ComponentStatus> DeserializeStatus(const std::string& json) const;
    std::string SerializeCommand(const Command& command) const;
    std::unique_ptr<Command> DeserializeCommand(const std::string& json) const;
    std::string SerializeCommandResponse(const CommandResponse& response) const;
    std::unique_ptr<CommandResponse> DeserializeCommandResponse(const std::string& json) const;
    
    // Phase 4: Enhanced JSON parsing methods  
    bool ParseEnhancedJSONSchema(const nlohmann::json& config, TransportConfig& transport_config);
    bool ParseDAQRoleConfiguration(const nlohmann::json& config, TransportConfig& transport_config);
    bool ParseTemplateConfiguration(const nlohmann::json& config, TransportConfig& transport_config);
    bool ParseLegacyJSONSchema(const nlohmann::json& config, TransportConfig& transport_config);
    bool ValidateJSONSchema(const nlohmann::json& config) const;
    bool ApplySmartDefaults(const std::string& daq_role, TransportConfig& transport_config) const;
    
    // Memory pool helper methods
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetContainerFromPool();
    void ReturnContainerToPool(std::unique_ptr<std::vector<std::unique_ptr<EventData>>> container);
};

} // namespace DELILA::Net