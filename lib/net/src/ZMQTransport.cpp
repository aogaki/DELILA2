#include "ZMQTransport.hpp"
#include <cstring>

namespace DELILA::Net {

ZMQTransport::ZMQTransport() {
    // Initialize ZMQ context following KISS principle
    fContext = std::make_unique<zmq::context_t>(1);
}

ZMQTransport::~ZMQTransport() {
    Disconnect();
}

bool ZMQTransport::IsConnected() const {
    return fConnected;
}

// GREEN: Minimal implementation to make tests pass
bool ZMQTransport::Configure(const TransportConfig& config) {
    // Simple validation following KISS principle
    if (config.data_address.empty()) {
        return false;  // Reject empty address
    }
    
    // Store valid configuration
    fConfig = config;
    fConfigured = true;
    return true;  // Accept valid configuration
}

bool ZMQTransport::Connect() {
    // Must be configured first
    if (!fConfigured) {
        return false;
    }
    
    try {
        if (fConfig.is_publisher) {
            // Publisher mode - create PUB socket for sending
            fDataSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_PUB);
            
            if (fConfig.bind_data) {
                fDataSocket->bind(fConfig.data_address);
            } else {
                fDataSocket->connect(fConfig.data_address);
            }
        } else {
            // Subscriber mode - create SUB socket for receiving
            fReceiveSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_SUB);
            
            // Subscribe to all messages (empty filter means accept all)
            fReceiveSocket->set(zmq::sockopt::subscribe, "");
            
            // Set receive timeout to avoid blocking indefinitely
            fReceiveSocket->set(zmq::sockopt::rcvtimeo, 1000); // 1 second timeout
            
            if (fConfig.bind_data) {
                fReceiveSocket->bind(fConfig.data_address);
            } else {
                fReceiveSocket->connect(fConfig.data_address);
            }
        }
        
        // Create status socket only when status address is different from data address
        // This avoids conflicts with existing tests
        if (fConfig.status_address != fConfig.data_address) {
            fStatusSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_REQ);
            fStatusSocket->set(zmq::sockopt::rcvtimeo, 1000); // 1 second timeout
            
            if (fConfig.bind_status) {
                fStatusSocket->bind(fConfig.status_address);
            } else {
                fStatusSocket->connect(fConfig.status_address);
            }
        }
        
        // Create command socket only when command address is different from data address
        if (fConfig.command_address != fConfig.data_address) {
            fCommandSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_REQ);
            fCommandSocket->set(zmq::sockopt::rcvtimeo, 1000); // 1 second timeout
            
            if (fConfig.bind_command) {
                fCommandSocket->bind(fConfig.command_address);
            } else {
                fCommandSocket->connect(fConfig.command_address);
            }
        }
        
        fConnected = true;
        return true;
        
    } catch (const zmq::error_t& e) {
        // Simple error handling following KISS
        fConnected = false;
        fDataSocket.reset();
        fReceiveSocket.reset();
        fStatusSocket.reset();
        fCommandSocket.reset();
        return false;
    }
}

void ZMQTransport::Disconnect() {
    fConnected = false;
    
    // Clean up sockets following KISS principle
    if (fDataSocket) {
        fDataSocket->close();
        fDataSocket.reset();
    }
    
    if (fReceiveSocket) {
        fReceiveSocket->close();
        fReceiveSocket.reset();
    }
    
    if (fStatusSocket) {
        fStatusSocket->close();
        fStatusSocket.reset();
    }
    
    if (fCommandSocket) {
        fCommandSocket->close();
        fCommandSocket.reset();
    }
}

bool ZMQTransport::Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events) {
    // Must be connected first
    if (!fConnected || !fDataSocket) {
        return false;
    }
    
    // Use existing Serializer to encode data with sequence number
    Serializer serializer;
    auto encoded_data = serializer.Encode(events, ++fSequenceNumber);
    
    if (!encoded_data || encoded_data->empty()) {
        return false;
    }
    
    try {
        // Create ZMQ message and send
        zmq::message_t message(encoded_data->size());
        std::memcpy(message.data(), encoded_data->data(), encoded_data->size());
        
        auto result = fDataSocket->send(message, zmq::send_flags::dontwait);
        return result.has_value();
        
    } catch (const zmq::error_t& e) {
        return false;
    }
}

std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> ZMQTransport::Receive() {
    // Must be connected first
    if (!fConnected || !fReceiveSocket) {
        return std::make_pair(nullptr, 0);
    }
    
    try {
        // Receive message with timeout
        zmq::message_t message;
        auto result = fReceiveSocket->recv(message, zmq::recv_flags::none);
        
        if (!result.has_value() || message.size() == 0) {
            // No message received or timeout
            return std::make_pair(nullptr, 0);
        }
        
        // Convert ZMQ message to vector
        auto received_data = std::make_unique<std::vector<uint8_t>>(
            static_cast<const uint8_t*>(message.data()),
            static_cast<const uint8_t*>(message.data()) + message.size());
        
        // Use Serializer to decode the data
        Serializer serializer;
        auto decoded_result = serializer.Decode(received_data);
        
        return decoded_result;
        
    } catch (const zmq::error_t& e) {
        // Simple error handling following KISS
        return std::make_pair(nullptr, 0);
    }
}

bool ZMQTransport::SendStatus(const ComponentStatus& status) {
    // Must be connected first
    if (!fConnected || !fStatusSocket) {
        return false;
    }
    
    try {
        // Serialize status to JSON
        std::string json = SerializeStatus(status);
        
        // Send as ZMQ message
        zmq::message_t message(json.size());
        std::memcpy(message.data(), json.c_str(), json.size());
        
        auto result = fStatusSocket->send(message, zmq::send_flags::dontwait);
        return result.has_value();
        
    } catch (const zmq::error_t& e) {
        return false;
    }
}

std::unique_ptr<ComponentStatus> ZMQTransport::ReceiveStatus() {
    // Must be connected first
    if (!fConnected || !fStatusSocket) {
        return nullptr;
    }
    
    try {
        // Receive message with timeout
        zmq::message_t message;
        auto result = fStatusSocket->recv(message, zmq::recv_flags::none);
        
        if (!result.has_value() || message.size() == 0) {
            return nullptr;
        }
        
        // Convert to string and deserialize
        std::string json(static_cast<const char*>(message.data()), message.size());
        return DeserializeStatus(json);
        
    } catch (const zmq::error_t& e) {
        return nullptr;
    }
}

CommandResponse ZMQTransport::SendCommand(const Command& command) {
    CommandResponse response;
    response.command_id = command.command_id;
    response.success = false;
    
    // Must be connected first and have command socket
    if (!fConnected || !fCommandSocket) {
        return response;
    }
    
    try {
        // Serialize command to JSON
        std::string json = SerializeCommand(command);
        
        // Send command as ZMQ message
        zmq::message_t message(json.size());
        std::memcpy(message.data(), json.c_str(), json.size());
        
        auto send_result = fCommandSocket->send(message, zmq::send_flags::none);
        if (!send_result.has_value()) {
            return response;
        }
        
        // Receive response
        zmq::message_t reply;
        auto recv_result = fCommandSocket->recv(reply, zmq::recv_flags::none);
        
        if (recv_result.has_value() && reply.size() > 0) {
            std::string reply_json(static_cast<const char*>(reply.data()), reply.size());
            auto parsed_response = DeserializeCommandResponse(reply_json);
            if (parsed_response) {
                return *parsed_response;
            }
        }
        
    } catch (const zmq::error_t& e) {
        // Simple error handling following KISS
    }
    
    return response;
}

std::unique_ptr<Command> ZMQTransport::ReceiveCommand() {
    // Must be connected first and have command socket
    if (!fConnected || !fCommandSocket) {
        return nullptr;
    }
    
    try {
        // Receive message with timeout
        zmq::message_t message;
        auto result = fCommandSocket->recv(message, zmq::recv_flags::none);
        
        if (!result.has_value() || message.size() == 0) {
            return nullptr;
        }
        
        // Convert to string and deserialize
        std::string json(static_cast<const char*>(message.data()), message.size());
        return DeserializeCommand(json);
        
    } catch (const zmq::error_t& e) {
        return nullptr;
    }
}

// Simple JSON serialization following KISS principle
std::string ZMQTransport::SerializeStatus(const ComponentStatus& status) const {
    std::ostringstream json;
    json << "{";
    json << "\"component_id\":\"" << status.component_id << "\",";
    json << "\"state\":\"" << status.state << "\",";
    json << "\"error_message\":\"" << status.error_message << "\",";
    json << "\"heartbeat_counter\":" << status.heartbeat_counter;
    json << "}";
    return json.str();
}

std::unique_ptr<ComponentStatus> ZMQTransport::DeserializeStatus(const std::string& json) const {
    // Very simple JSON parsing - not production ready but follows KISS principle
    auto status = std::make_unique<ComponentStatus>();
    
    // Extract component_id
    size_t pos = json.find("\"component_id\":\"");
    if (pos != std::string::npos) {
        pos += 16; // Length of "component_id":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            status->component_id = json.substr(pos, end - pos);
        }
    }
    
    // Extract state
    pos = json.find("\"state\":\"");
    if (pos != std::string::npos) {
        pos += 9; // Length of "state":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            status->state = json.substr(pos, end - pos);
        }
    }
    
    // Extract error_message
    pos = json.find("\"error_message\":\"");
    if (pos != std::string::npos) {
        pos += 17; // Length of "error_message":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            status->error_message = json.substr(pos, end - pos);
        }
    }
    
    // Extract heartbeat_counter
    pos = json.find("\"heartbeat_counter\":");
    if (pos != std::string::npos) {
        pos += 20; // Length of "heartbeat_counter":
        size_t end = json.find_first_not_of("0123456789", pos);
        if (end != std::string::npos) {
            std::string counter_str = json.substr(pos, end - pos);
            status->heartbeat_counter = std::stoull(counter_str);
        }
    }
    
    return status;
}

// Command serialization methods
std::string ZMQTransport::SerializeCommand(const Command& command) const {
    std::ostringstream json;
    json << "{";
    json << "\"command_id\":\"" << command.command_id << "\",";
    json << "\"type\":" << static_cast<int>(command.type) << ",";
    json << "\"target_component\":\"" << command.target_component << "\"";
    json << "}";
    return json.str();
}

std::unique_ptr<Command> ZMQTransport::DeserializeCommand(const std::string& json) const {
    auto command = std::make_unique<Command>();
    
    // Extract command_id
    size_t pos = json.find("\"command_id\":\"");
    if (pos != std::string::npos) {
        pos += 14; // Length of "command_id":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            command->command_id = json.substr(pos, end - pos);
        }
    }
    
    // Extract type
    pos = json.find("\"type\":");
    if (pos != std::string::npos) {
        pos += 7; // Length of "type":
        size_t end = json.find_first_not_of("0123456789", pos);
        if (end != std::string::npos) {
            std::string type_str = json.substr(pos, end - pos);
            command->type = static_cast<CommandType>(std::stoi(type_str));
        }
    }
    
    // Extract target_component
    pos = json.find("\"target_component\":\"");
    if (pos != std::string::npos) {
        pos += 20; // Length of "target_component":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            command->target_component = json.substr(pos, end - pos);
        }
    }
    
    return command;
}

std::string ZMQTransport::SerializeCommandResponse(const CommandResponse& response) const {
    std::ostringstream json;
    json << "{";
    json << "\"command_id\":\"" << response.command_id << "\",";
    json << "\"success\":" << (response.success ? "true" : "false") << ",";
    json << "\"message\":\"" << response.message << "\"";
    json << "}";
    return json.str();
}

std::unique_ptr<CommandResponse> ZMQTransport::DeserializeCommandResponse(const std::string& json) const {
    auto response = std::make_unique<CommandResponse>();
    
    // Extract command_id
    size_t pos = json.find("\"command_id\":\"");
    if (pos != std::string::npos) {
        pos += 14; // Length of "command_id":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            response->command_id = json.substr(pos, end - pos);
        }
    }
    
    // Extract success
    pos = json.find("\"success\":");
    if (pos != std::string::npos) {
        pos += 10; // Length of "success":
        response->success = (json.substr(pos, 4) == "true");
    }
    
    // Extract message
    pos = json.find("\"message\":\"");
    if (pos != std::string::npos) {
        pos += 11; // Length of "message":"
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            response->message = json.substr(pos, end - pos);
        }
    }
    
    return response;
}

} // namespace DELILA::Net