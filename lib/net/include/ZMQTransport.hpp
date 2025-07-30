#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <sstream>
#include <zmq.hpp>
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

private:
    bool fConnected = false;
    bool fConfigured = false;
    TransportConfig fConfig;
    uint64_t fSequenceNumber = 0;
    
    // ZeroMQ context and sockets (following KISS - start with data only)
    std::unique_ptr<zmq::context_t> fContext;
    std::unique_ptr<zmq::socket_t> fDataSocket;  // PUB socket for sending
    std::unique_ptr<zmq::socket_t> fReceiveSocket;  // SUB socket for receiving
    std::unique_ptr<zmq::socket_t> fStatusSocket;  // For status communication
    std::unique_ptr<zmq::socket_t> fCommandSocket;  // For command communication
    
    // Helper methods for simple JSON serialization (KISS approach)
    std::string SerializeStatus(const ComponentStatus& status) const;
    std::unique_ptr<ComponentStatus> DeserializeStatus(const std::string& json) const;
    std::string SerializeCommand(const Command& command) const;
    std::unique_ptr<Command> DeserializeCommand(const std::string& json) const;
    std::string SerializeCommandResponse(const CommandResponse& response) const;
    std::unique_ptr<CommandResponse> DeserializeCommandResponse(const std::string& json) const;
};

} // namespace DELILA::Net