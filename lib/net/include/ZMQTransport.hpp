/**
 * @file ZMQTransport.hpp
 * @brief ZeroMQ-based transport layer for DELILA2 network communication
 * @author DELILA2 Development Team  
 * @date 2024
 */

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "../../core/include/delila/core/Command.hpp"
#include "../../core/include/delila/core/CommandResponse.hpp"
#include "../../core/include/delila/core/ComponentState.hpp"

// Forward declarations
namespace DELILA::Digitizer
{
class EventData;
class MinimalEventData;
}  // namespace DELILA::Digitizer

/**
 * @brief Network library namespace containing transport and serialization components
 */
namespace DELILA::Net
{

/**
 * @brief Configuration structure for ZMQ transport layer
 * 
 * Simple configuration following KISS principle. Provides essential
 * settings for data, status, and command channels.
 * 
 * @par Usage Example:
 * @code{.cpp}
 * TransportConfig config;
 * config.data_address = "tcp://*:5555";
 * config.bind_data = true;
 * config.data_pattern = "PUB";
 * config.is_publisher = true;
 * 
 * ZMQTransport transport;
 * transport.Configure(config);
 * @endcode
 */
struct TransportConfig {
  std::string data_address = "tcp://localhost:5555";      ///< Data channel endpoint
  std::string status_address = "tcp://localhost:5556";    ///< Status channel endpoint  
  std::string command_address = "tcp://localhost:5557";   ///< Command channel endpoint
  bool bind_data = true;        ///< True to bind data socket, false to connect
  bool bind_status = true;      ///< True to bind status socket, false to connect
  bool bind_command = false;    ///< True to bind command socket, false to connect

  /**
   * @brief Pattern specification for data channel
   * 
   * Supported patterns:
   * - "PUB": Publisher socket (1-to-many broadcast)
   * - "SUB": Subscriber socket (receive broadcasts)  
   * - "PUSH": Push socket (load-balanced distribution)
   * - "PULL": Pull socket (receive load-balanced messages)
   */
  std::string data_pattern = "PUB";

  /**
   * @brief Role specification for PUB/SUB pattern
   * 
   * - true: PUB role (send data)
   * - false: SUB role (receive data)
   * 
   * @note Only relevant when data_pattern is "PUB" or "SUB"
   */
  bool is_publisher = true;
};

/**
 * @brief Component status structure for health monitoring
 * 
 * Used for system health monitoring and diagnostics. Provides
 * comprehensive status information including metrics and error states.
 * 
 * @par Usage Example:
 * @code{.cpp}
 * ComponentStatus status;
 * status.component_id = "digitizer_01";
 * status.state = "ACQUIRING";
 * status.timestamp = std::chrono::system_clock::now();
 * status.metrics["events_per_second"] = 125000.0;
 * status.heartbeat_counter++;
 * 
 * transport.SendStatus(status);
 * @endcode
 */
struct ComponentStatus {
  std::string component_id;                               ///< Unique component identifier
  std::string state;                                      ///< Current operational state
  std::chrono::system_clock::time_point timestamp;       ///< Status timestamp
  std::map<std::string, double> metrics;                 ///< Performance metrics
  std::string error_message;                             ///< Last error message (if any)
  uint64_t heartbeat_counter = 0;                        ///< Incremental heartbeat counter
};

// KISS: Simple, focused interface
class ZMQTransport
{
 public:
  ZMQTransport();
  virtual ~ZMQTransport();

  // Connection management
  bool Configure(const TransportConfig &config);
  bool ConfigureFromJSON(const nlohmann::json &config);
  bool ConfigureFromFile(const std::string &filename);
  bool Connect();
  void Disconnect();
  bool IsConnected() const;

  // New byte-based transport methods (pure transport layer)
  bool SendBytes(std::unique_ptr<std::vector<uint8_t>> &data);
  std::unique_ptr<std::vector<uint8_t>> ReceiveBytes();

  // Status functions
  bool SendStatus(const ComponentStatus &status);
  std::unique_ptr<ComponentStatus> ReceiveStatus();

  // Command functions (REQ/REP pattern)
  // For Operator side (REQ) - send command and wait for response
  std::optional<DELILA::CommandResponse> SendCommand(
      const DELILA::Command &cmd,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

  // For Component side (REP) - receive command and send response
  std::optional<DELILA::Command> ReceiveCommand(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
  bool SendCommandResponse(const DELILA::CommandResponse &response);

 private:
  bool fConnected = false;
  bool fConfigured = false;
  TransportConfig fConfig;

  // ZeroMQ context and sockets (KISS - byte transport only)
  std::unique_ptr<zmq::context_t> fContext;
  std::unique_ptr<zmq::socket_t> fDataSocket;     // Data socket
  std::unique_ptr<zmq::socket_t> fStatusSocket;   // For status communication
  std::unique_ptr<zmq::socket_t> fCommandSocket;  // For command REQ/REP

  // Helper methods for JSON status serialization
  std::string SerializeStatus(const ComponentStatus &status) const;
  std::unique_ptr<ComponentStatus> DeserializeStatus(
      const std::string &json) const;

  // Helper methods for JSON command serialization
  std::string SerializeCommand(const DELILA::Command &cmd) const;
  std::optional<DELILA::Command> DeserializeCommand(const std::string &json) const;
  std::string SerializeCommandResponse(
      const DELILA::CommandResponse &response) const;
  std::optional<DELILA::CommandResponse> DeserializeCommandResponse(
      const std::string &json) const;
};

}  // namespace DELILA::Net