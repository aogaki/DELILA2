#include "ZMQTransport.hpp"

#include <cstring>
#include <fstream>

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
  if (config.data_address.empty()) {
    return false;  // Reject empty address
  }

  // Validate data pattern
  if (config.data_pattern != "PUB" && config.data_pattern != "SUB" &&
      config.data_pattern != "PUSH" && config.data_pattern != "PULL" &&
      config.data_pattern != "DEALER" && config.data_pattern != "ROUTER" &&
      config.data_pattern != "PAIR") {
    return false;  // Reject invalid pattern
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

      // Set receive timeout
      fDataSocket->set(zmq::sockopt::rcvtimeo, 1000);  // 1 second timeout

      if (fConfig.bind_data) {
        fDataSocket->bind(fConfig.data_address);
      } else {
        fDataSocket->connect(fConfig.data_address);
      }
    }

    // Create status socket only when status address is different from data address
    if (fConfig.status_address != fConfig.data_address) {
      fStatusSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_REQ);
      fStatusSocket->set(zmq::sockopt::rcvtimeo, 1000);  // 1 second timeout

      if (fConfig.bind_status) {
        fStatusSocket->bind(fConfig.status_address);
      } else {
        fStatusSocket->connect(fConfig.status_address);
      }
    }

    fConnected = true;
    return true;

  } catch (const zmq::error_t &e) {
    // Simple error handling following KISS
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

}  // namespace DELILA::Net