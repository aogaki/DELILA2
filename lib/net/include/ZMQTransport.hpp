#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <sstream>
#include <queue>
#include <mutex>
#include <variant>
#include <zmq.hpp>
#include <nlohmann/json.hpp>

// Forward declarations
namespace DELILA::Digitizer {
    class EventData;
    class MinimalEventData;
}
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
    
    // Role specification for PUB/SUB pattern
    bool is_publisher = true;  // true = PUB (send), false = SUB (receive)
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
    
    // New byte-based transport methods (pure transport layer)
    bool SendBytes(std::unique_ptr<std::vector<uint8_t>>& data);
    std::unique_ptr<std::vector<uint8_t>> ReceiveBytes();
    
    
    // Status functions  
    bool SendStatus(const ComponentStatus& status);
    std::unique_ptr<ComponentStatus> ReceiveStatus();
    
    

private:
    bool fConnected = false;
    bool fConfigured = false;
    TransportConfig fConfig;
    
    // ZeroMQ context and sockets (KISS - byte transport only)
    std::unique_ptr<zmq::context_t> fContext;
    std::unique_ptr<zmq::socket_t> fDataSocket;  // Data socket
    std::unique_ptr<zmq::socket_t> fStatusSocket;  // For status communication
    
    // Helper methods for JSON status serialization
    std::string SerializeStatus(const ComponentStatus& status) const;
    std::unique_ptr<ComponentStatus> DeserializeStatus(const std::string& json) const;
};

} // namespace DELILA::Net