#ifndef DELILA_CORE_COMPONENT_CONFIG_HPP
#define DELILA_CORE_COMPONENT_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace DELILA {

/**
 * @brief Network transport configuration
 *
 * Configures ZeroMQ sockets for data, status, and command channels.
 */
struct TransportConfig {
  std::string data_address;    ///< Data channel address (e.g., "tcp://host:5555")
  std::string status_address;  ///< Status channel address
  std::string command_address; ///< Command channel address

  bool bind_data = false;    ///< true: bind, false: connect for data channel
  bool bind_status = false;  ///< true: bind, false: connect for status channel
  bool bind_command = false; ///< true: bind, false: connect for command channel

  std::string data_pattern; ///< Data pattern: "PUSH", "PULL", "PUB", "SUB"
};

/**
 * @brief Configuration for a DataComponent
 *
 * Contains all settings needed to initialize and run a DataComponent.
 * Loaded from JSON configuration file or MongoDB.
 *
 * Example JSON:
 *   {
 *     "component_id": "source_01",
 *     "component_type": "DigitizerSource",
 *     "input_addresses": [],
 *     "output_addresses": ["tcp://localhost:5555"],
 *     "transport": {
 *       "data_address": "tcp://*:5555",
 *       "bind_data": true,
 *       "data_pattern": "PUSH"
 *     },
 *     "queue_max_size": 10000,
 *     "queue_warning_threshold": 8000,
 *     "status_interval_ms": 1000,
 *     "command_timeout_ms": 5000
 *   }
 */
struct ComponentConfig {
  std::string component_id;   ///< Unique identifier (e.g., "source_01")
  std::string component_type; ///< Type name (e.g., "DigitizerSource")

  // Input/Output addresses
  std::vector<std::string> input_addresses;  ///< Addresses to receive data from
  std::vector<std::string> output_addresses; ///< Addresses to send data to

  // Transport configuration
  TransportConfig transport; ///< Network transport settings

  // Buffer settings
  uint32_t queue_max_size = 0;         ///< Maximum queue size (0 = default)
  uint32_t queue_warning_threshold = 0; ///< Warning threshold for queue fill

  // Timing settings
  uint32_t status_interval_ms = 0;  ///< Status report interval (ms)
  uint32_t command_timeout_ms = 0;  ///< Command response timeout (ms)
};

} // namespace DELILA

#endif // DELILA_CORE_COMPONENT_CONFIG_HPP
