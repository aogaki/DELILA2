#include "ZMQTransport.hpp"

#include <cstring>
#include <fstream>
#include <iostream>

namespace DELILA::Net
{

ZMQTransport::ZMQTransport()
{
  // Initialize ZMQ context following KISS principle
  fContext = std::make_unique<zmq::context_t>(1);
}

ZMQTransport::~ZMQTransport() { Disconnect(); }

bool ZMQTransport::IsConnected() const { return fConnected; }

bool ZMQTransport::Configure(const TransportConfig &config)
{
  // Simple validation following KISS principle
  // Allow command-only transport (data_address empty but command_address set)
  bool has_data = !config.data_address.empty();
  bool has_command = !config.command_address.empty();

  if (!has_data && !has_command) {
    return false;  // Reject completely empty configuration
  }

  // Validate data pattern only if data channel is configured
  if (has_data) {
    if (config.data_pattern != "PUB" && config.data_pattern != "SUB" &&
        config.data_pattern != "PUSH" && config.data_pattern != "PULL" &&
        config.data_pattern != "DEALER" && config.data_pattern != "ROUTER" &&
        config.data_pattern != "PAIR") {
      return false;  // Reject invalid pattern
    }
  }

  // Store valid configuration
  fConfig = config;
  fConfigured = true;
  return true;  // Accept valid configuration
}

bool ZMQTransport::ConfigureFromJSON(const nlohmann::json &config)
{
  try {
    // Create TransportConfig from JSON (KISS approach)
    TransportConfig transport_config;

    // Simple JSON parsing for essential fields only
    if (config.contains("data_address")) {
      transport_config.data_address = config["data_address"];
    }
    if (config.contains("status_address")) {
      transport_config.status_address = config["status_address"];
    }
    if (config.contains("data_pattern")) {
      transport_config.data_pattern = config["data_pattern"];
    }
    if (config.contains("bind_data")) {
      transport_config.bind_data = config["bind_data"];
    }
    if (config.contains("bind_status")) {
      transport_config.bind_status = config["bind_status"];
    }
    if (config.contains("is_publisher")) {
      transport_config.is_publisher = config["is_publisher"];
    }
    if (config.contains("command_address")) {
      transport_config.command_address = config["command_address"];
    }
    if (config.contains("bind_command")) {
      transport_config.bind_command = config["bind_command"];
    }

    return Configure(transport_config);

  } catch (const nlohmann::json::exception &e) {
    return false;
  } catch (...) {
    return false;
  }
}

bool ZMQTransport::ConfigureFromFile(const std::string &filename)
{
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      return false;  // File not found or not readable
    }

    std::string file_content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    if (file_content.empty()) {
      return false;  // Empty file
    }

    nlohmann::json config = nlohmann::json::parse(file_content);
    return ConfigureFromJSON(config);

  } catch (const nlohmann::json::parse_error &e) {
    return false;
  } catch (const std::exception &e) {
    return false;
  } catch (...) {
    return false;
  }
}

bool ZMQTransport::Connect()
{
  // Must be configured first
  if (!fConfigured) {
    return false;
  }

  try {
    // Only create data socket if data_address is configured
    if (!fConfig.data_address.empty()) {
      // Determine effective pattern
      std::string effective_pattern = fConfig.data_pattern;

      // Backward compatibility: If still using default "PUB", determine pattern from is_publisher
      if (effective_pattern == "PUB" && !fConfig.is_publisher) {
        effective_pattern = "SUB";
      }

      // Create sockets based on effective pattern
      if (effective_pattern == "PUB" || effective_pattern == "PUSH" ||
          effective_pattern == "DEALER") {
        // Sending patterns
        int socket_type;
        if (effective_pattern == "PUB")
          socket_type = ZMQ_PUB;
        else if (effective_pattern == "PUSH")
          socket_type = ZMQ_PUSH;
        else
          socket_type = ZMQ_DEALER;

        fDataSocket = std::make_unique<zmq::socket_t>(*fContext, socket_type);
        fDataSocket->set(zmq::sockopt::linger, 0);  // Don't linger on close

        if (fConfig.bind_data) {
          fDataSocket->bind(fConfig.data_address);
        } else {
          fDataSocket->connect(fConfig.data_address);
        }
      } else if (effective_pattern == "SUB" || effective_pattern == "PULL" ||
                 effective_pattern == "ROUTER") {
        // Receiving patterns
        int socket_type;
        if (effective_pattern == "SUB")
          socket_type = ZMQ_SUB;
        else if (effective_pattern == "PULL")
          socket_type = ZMQ_PULL;
        else
          socket_type = ZMQ_ROUTER;

        fDataSocket = std::make_unique<zmq::socket_t>(*fContext, socket_type);
        fDataSocket->set(zmq::sockopt::linger, 0);  // Don't linger on close

        // SUB sockets need subscription filter
        if (effective_pattern == "SUB") {
          fDataSocket->set(zmq::sockopt::subscribe, "");  // Accept all messages
        }

        // Set receive timeout to avoid blocking indefinitely
        fDataSocket->set(zmq::sockopt::rcvtimeo, 1000);  // 1 second timeout

        if (fConfig.bind_data) {
          fDataSocket->bind(fConfig.data_address);
        } else {
          fDataSocket->connect(fConfig.data_address);
        }
      } else if (effective_pattern == "PAIR") {
        // PAIR pattern: Exclusive 1:1 communication (both send and receive)
        fDataSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_PAIR);
        fDataSocket->set(zmq::sockopt::linger, 0);  // Don't linger on close

        // Set receive timeout
        fDataSocket->set(zmq::sockopt::rcvtimeo, 1000);  // 1 second timeout

        if (fConfig.bind_data) {
          fDataSocket->bind(fConfig.data_address);
        } else {
          fDataSocket->connect(fConfig.data_address);
        }
      }
    }

    // Create status socket only when status address is set and different from data address
    if (!fConfig.status_address.empty() && fConfig.status_address != fConfig.data_address) {
      fStatusSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_REQ);
      fStatusSocket->set(zmq::sockopt::linger, 0);  // Don't linger on close
      fStatusSocket->set(zmq::sockopt::rcvtimeo, 1000);  // 1 second timeout

      if (fConfig.bind_status) {
        fStatusSocket->bind(fConfig.status_address);
      } else {
        fStatusSocket->connect(fConfig.status_address);
      }
    }

    // Create command socket (REQ/REP pattern)
    // REQ for client (Operator), REP for server (Component)
    if (!fConfig.command_address.empty()) {
      int cmd_socket_type = fConfig.bind_command ? ZMQ_REP : ZMQ_REQ;
      fCommandSocket = std::make_unique<zmq::socket_t>(*fContext, cmd_socket_type);
      fCommandSocket->set(zmq::sockopt::linger, 0);  // Don't linger on close
      fCommandSocket->set(zmq::sockopt::rcvtimeo, 1000);  // 1 second default timeout

      if (fConfig.bind_command) {
        fCommandSocket->bind(fConfig.command_address);
      } else {
        fCommandSocket->connect(fConfig.command_address);
      }
    }

    fConnected = true;
    return true;

  } catch (const zmq::error_t &e) {
    // Enhanced error handling with diagnostics for testing
    #ifdef DEBUG
    std::cerr << "ZMQ Connection Error: " << e.what() << " (errno: " << e.num() 
              << ", address: " << fConfig.data_address << ")" << std::endl;
    #endif
    fConnected = false;
    fDataSocket.reset();
    fStatusSocket.reset();
    return false;
  } catch (const std::exception &e) {
    #ifdef DEBUG
    std::cerr << "ZMQ Connection Error (std::exception): " << e.what() << std::endl;
    #endif
    fConnected = false;
    fDataSocket.reset();
    fStatusSocket.reset();
    return false;
  } catch (...) {
    #ifdef DEBUG
    std::cerr << "ZMQ Connection Error: Unknown exception" << std::endl;
    #endif
    fConnected = false;
    fDataSocket.reset();
    fStatusSocket.reset();
    return false;
  }
}

void ZMQTransport::Disconnect()
{
  fConnected = false;

  // Clean up sockets following KISS principle
  if (fDataSocket) {
    fDataSocket->close();
    fDataSocket.reset();
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

// Core byte-based transport implementation
bool ZMQTransport::SendBytes(std::unique_ptr<std::vector<uint8_t>> &data)
{
  // Must be connected first
  if (!fConnected || !fDataSocket) {
    return false;
  }

  // Check for null or empty data
  if (!data || data->empty()) {
    return false;
  }

  try {
    // Create message and copy data
    zmq::message_t message(data->size());
    std::memcpy(message.data(), data->data(), data->size());

    // Clear the original data to indicate ownership transfer
    data.reset();

    auto result = fDataSocket->send(message, zmq::send_flags::dontwait);
    return result.has_value();

  } catch (const zmq::error_t &e) {
    return false;
  }
}

std::unique_ptr<std::vector<uint8_t>> ZMQTransport::ReceiveBytes()
{
  // Must be connected first
  if (!fConnected || !fDataSocket) {
    return nullptr;
  }

  try {
    // Receive message with timeout
    zmq::message_t message;
    auto result = fDataSocket->recv(message, zmq::recv_flags::none);

    if (!result.has_value() || message.size() == 0) {
      // No message received or timeout
      return nullptr;
    }

    // Convert ZMQ message to vector
    auto received_data = std::make_unique<std::vector<uint8_t>>(
        static_cast<const uint8_t *>(message.data()),
        static_cast<const uint8_t *>(message.data()) + message.size());

    return received_data;

  } catch (const zmq::error_t &e) {
    // Simple error handling following KISS
    return nullptr;
  }
}

bool ZMQTransport::SendStatus(const ComponentStatus &status)
{
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

  } catch (const zmq::error_t &e) {
    return false;
  }
}

std::unique_ptr<ComponentStatus> ZMQTransport::ReceiveStatus()
{
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
    std::string json(static_cast<const char *>(message.data()), message.size());
    return DeserializeStatus(json);

  } catch (const zmq::error_t &e) {
    return nullptr;
  }
}

// Simple JSON serialization following KISS principle
std::string ZMQTransport::SerializeStatus(const ComponentStatus &status) const
{
  std::ostringstream json;
  json << "{";
  json << "\"component_id\":\"" << status.component_id << "\",";
  json << "\"state\":\"" << status.state << "\",";
  json << "\"error_message\":\"" << status.error_message << "\",";
  json << "\"heartbeat_counter\":" << status.heartbeat_counter;

  // Serialize metrics map
  if (!status.metrics.empty()) {
    json << ",\"metrics\":{";
    bool first = true;
    for (const auto &[key, value] : status.metrics) {
      if (!first) json << ",";
      json << "\"" << key << "\":" << value;
      first = false;
    }
    json << "}";
  }
  json << "}";
  return json.str();
}

std::unique_ptr<ComponentStatus> ZMQTransport::DeserializeStatus(
    const std::string &json) const
{
  // Very simple JSON parsing - follows KISS principle
  auto status = std::make_unique<ComponentStatus>();

  // Extract component_id
  size_t pos = json.find("\"component_id\":\"");
  if (pos != std::string::npos) {
    pos += 16;  // Length of "component_id":"
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      status->component_id = json.substr(pos, end - pos);
    }
  }

  // Extract state
  pos = json.find("\"state\":\"");
  if (pos != std::string::npos) {
    pos += 9;  // Length of "state":"
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      status->state = json.substr(pos, end - pos);
    }
  }

  // Extract error_message
  pos = json.find("\"error_message\":\"");
  if (pos != std::string::npos) {
    pos += 17;  // Length of "error_message":"
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      status->error_message = json.substr(pos, end - pos);
    }
  }

  // Extract heartbeat_counter
  pos = json.find("\"heartbeat_counter\":");
  if (pos != std::string::npos) {
    pos += 20;  // Length of "heartbeat_counter":
    size_t end = json.find_first_not_of("0123456789", pos);
    if (end != std::string::npos) {
      std::string counter_str = json.substr(pos, end - pos);
      status->heartbeat_counter = std::stoull(counter_str);
    }
  }

  return status;
}

// Command channel implementation (REQ/REP pattern)
std::optional<DELILA::CommandResponse> ZMQTransport::SendCommand(
    const DELILA::Command &cmd, std::chrono::milliseconds timeout)
{
  if (!fConnected || !fCommandSocket) {
    return std::nullopt;
  }

  try {
    // Set timeout for this operation
    fCommandSocket->set(zmq::sockopt::rcvtimeo,
                        static_cast<int>(timeout.count()));

    // Serialize and send command
    std::string json = SerializeCommand(cmd);
    zmq::message_t request(json.size());
    std::memcpy(request.data(), json.c_str(), json.size());

    auto send_result = fCommandSocket->send(request, zmq::send_flags::none);
    if (!send_result.has_value()) {
      return std::nullopt;
    }

    // Wait for response
    zmq::message_t reply;
    auto recv_result = fCommandSocket->recv(reply, zmq::recv_flags::none);
    if (!recv_result.has_value() || reply.size() == 0) {
      return std::nullopt;
    }

    std::string reply_json(static_cast<const char *>(reply.data()),
                           reply.size());
    return DeserializeCommandResponse(reply_json);

  } catch (const zmq::error_t &e) {
    return std::nullopt;
  }
}

std::optional<DELILA::Command> ZMQTransport::ReceiveCommand(
    std::chrono::milliseconds timeout)
{
  if (!fConnected || !fCommandSocket) {
    return std::nullopt;
  }

  try {
    // Set timeout
    fCommandSocket->set(zmq::sockopt::rcvtimeo,
                        static_cast<int>(timeout.count()));

    zmq::message_t request;
    auto result = fCommandSocket->recv(request, zmq::recv_flags::none);

    if (!result.has_value() || request.size() == 0) {
      return std::nullopt;
    }

    std::string json(static_cast<const char *>(request.data()), request.size());
    return DeserializeCommand(json);

  } catch (const zmq::error_t &e) {
    return std::nullopt;
  }
}

bool ZMQTransport::SendCommandResponse(const DELILA::CommandResponse &response)
{
  if (!fConnected || !fCommandSocket) {
    return false;
  }

  try {
    std::string json = SerializeCommandResponse(response);
    zmq::message_t reply(json.size());
    std::memcpy(reply.data(), json.c_str(), json.size());

    auto result = fCommandSocket->send(reply, zmq::send_flags::none);
    return result.has_value();

  } catch (const zmq::error_t &e) {
    return false;
  }
}

// JSON serialization for Command
std::string ZMQTransport::SerializeCommand(const DELILA::Command &cmd) const
{
  std::ostringstream json;
  json << "{";
  json << "\"type\":" << static_cast<int>(cmd.type) << ",";
  json << "\"request_id\":" << cmd.request_id << ",";
  json << "\"run_number\":" << cmd.run_number << ",";
  json << "\"graceful\":" << (cmd.graceful ? "true" : "false") << ",";
  json << "\"config_path\":\"" << cmd.config_path << "\",";
  json << "\"payload\":\"" << cmd.payload << "\"";
  json << "}";
  return json.str();
}

std::optional<DELILA::Command> ZMQTransport::DeserializeCommand(
    const std::string &json) const
{
  DELILA::Command cmd;

  // Extract type
  size_t pos = json.find("\"type\":");
  if (pos != std::string::npos) {
    pos += 7;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      int type_val = std::stoi(json.substr(pos, end - pos));
      cmd.type = static_cast<DELILA::CommandType>(type_val);
    }
  }

  // Extract request_id
  pos = json.find("\"request_id\":");
  if (pos != std::string::npos) {
    pos += 13;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      cmd.request_id = std::stoul(json.substr(pos, end - pos));
    }
  }

  // Extract run_number
  pos = json.find("\"run_number\":");
  if (pos != std::string::npos) {
    pos += 13;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      cmd.run_number = std::stoul(json.substr(pos, end - pos));
    }
  }

  // Extract graceful
  pos = json.find("\"graceful\":");
  if (pos != std::string::npos) {
    pos += 11;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      std::string val = json.substr(pos, end - pos);
      cmd.graceful = (val == "true");
    }
  }

  // Extract config_path
  pos = json.find("\"config_path\":\"");
  if (pos != std::string::npos) {
    pos += 15;
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      cmd.config_path = json.substr(pos, end - pos);
    }
  }

  // Extract payload
  pos = json.find("\"payload\":\"");
  if (pos != std::string::npos) {
    pos += 11;
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      cmd.payload = json.substr(pos, end - pos);
    }
  }

  return cmd;
}

std::string ZMQTransport::SerializeCommandResponse(
    const DELILA::CommandResponse &response) const
{
  std::ostringstream json;
  json << "{";
  json << "\"request_id\":" << response.request_id << ",";
  json << "\"success\":" << (response.success ? "true" : "false") << ",";
  json << "\"error_code\":" << static_cast<int>(response.error_code) << ",";
  json << "\"current_state\":" << static_cast<int>(response.current_state) << ",";
  json << "\"message\":\"" << response.message << "\",";
  json << "\"payload\":\"" << response.payload << "\"";
  json << "}";
  return json.str();
}

std::optional<DELILA::CommandResponse> ZMQTransport::DeserializeCommandResponse(
    const std::string &json) const
{
  DELILA::CommandResponse response;

  // Extract request_id
  size_t pos = json.find("\"request_id\":");
  if (pos != std::string::npos) {
    pos += 13;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      response.request_id = std::stoul(json.substr(pos, end - pos));
    }
  }

  // Extract success
  pos = json.find("\"success\":");
  if (pos != std::string::npos) {
    pos += 10;
    response.success = (json.substr(pos, 4) == "true");
  }

  // Extract error_code
  pos = json.find("\"error_code\":");
  if (pos != std::string::npos) {
    pos += 13;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      int code_val = std::stoi(json.substr(pos, end - pos));
      response.error_code = static_cast<DELILA::ErrorCode>(code_val);
    }
  }

  // Extract current_state
  pos = json.find("\"current_state\":");
  if (pos != std::string::npos) {
    pos += 16;
    size_t end = json.find_first_of(",}", pos);
    if (end != std::string::npos) {
      int state_val = std::stoi(json.substr(pos, end - pos));
      response.current_state = static_cast<DELILA::ComponentState>(state_val);
    }
  }

  // Extract message
  pos = json.find("\"message\":\"");
  if (pos != std::string::npos) {
    pos += 11;
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      response.message = json.substr(pos, end - pos);
    }
  }

  // Extract payload
  pos = json.find("\"payload\":\"");
  if (pos != std::string::npos) {
    pos += 11;
    size_t end = json.find("\"", pos);
    if (end != std::string::npos) {
      response.payload = json.substr(pos, end - pos);
    }
  }

  return response;
}

}  // namespace DELILA::Net