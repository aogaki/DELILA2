#ifndef DELILA_CORE_COMMAND_HPP
#define DELILA_CORE_COMMAND_HPP

#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief Command types for DAQ control
 */
enum class CommandType : uint8_t {
  // Lifecycle commands
  Configure = 0, ///< Load configuration from file
  Arm = 1,       ///< Prepare for data acquisition
  Start = 2,     ///< Start data acquisition
  Stop = 3,      ///< Stop data acquisition
  Reset = 4,     ///< Reset to Idle state

  // Query commands
  GetStatus = 10, ///< Request current status
  GetConfig = 11, ///< Request current configuration

  // Utility commands
  Ping = 20 ///< Check if component is alive
};

/**
 * @brief Command structure for REQ/REP communication
 */
struct Command {
  CommandType type;       ///< Command type
  uint32_t request_id;    ///< Unique request ID for correlation
  std::string config_path; ///< Configuration file path (for Configure command)
  std::string payload;    ///< Additional command payload (JSON)

  Command() : type(CommandType::GetStatus), request_id(0) {}

  Command(CommandType t, uint32_t id = 0)
      : type(t), request_id(id) {}
};

/**
 * @brief Convert CommandType to string
 */
inline std::string CommandTypeToString(CommandType type) {
  switch (type) {
  case CommandType::Configure:
    return "Configure";
  case CommandType::Arm:
    return "Arm";
  case CommandType::Start:
    return "Start";
  case CommandType::Stop:
    return "Stop";
  case CommandType::Reset:
    return "Reset";
  case CommandType::GetStatus:
    return "GetStatus";
  case CommandType::GetConfig:
    return "GetConfig";
  case CommandType::Ping:
    return "Ping";
  default:
    return "Unknown";
  }
}

} // namespace DELILA

#endif // DELILA_CORE_COMMAND_HPP
