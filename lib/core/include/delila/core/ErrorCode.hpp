#ifndef DELILA_CORE_ERROR_CODE_HPP
#define DELILA_CORE_ERROR_CODE_HPP

#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief Error codes for command responses and component operations
 */
enum class ErrorCode : uint16_t {
  Success = 0,

  // Configuration errors (100-199)
  InvalidConfiguration = 100,
  ConfigurationNotFound = 101,
  ConfigurationValidationFailed = 102,

  // State errors (200-299)
  InvalidStateTransition = 200,
  NotConfigured = 201,
  NotArmed = 202,
  AlreadyRunning = 203,

  // Hardware errors (300-399)
  HardwareNotFound = 300,
  HardwareConnectionFailed = 301,
  HardwareTimeout = 302,

  // Communication errors (400-499)
  CommunicationError = 400,
  Timeout = 401,
  ConnectionLost = 402,

  // Internal errors (500-599)
  InternalError = 500,
  OutOfMemory = 501,

  // Unknown error
  Unknown = 999
};

/**
 * @brief Convert ErrorCode to string for logging/debugging
 */
inline std::string ErrorCodeToString(ErrorCode code) {
  switch (code) {
  case ErrorCode::Success:
    return "Success";
  case ErrorCode::InvalidConfiguration:
    return "InvalidConfiguration";
  case ErrorCode::ConfigurationNotFound:
    return "ConfigurationNotFound";
  case ErrorCode::ConfigurationValidationFailed:
    return "ConfigurationValidationFailed";
  case ErrorCode::InvalidStateTransition:
    return "InvalidStateTransition";
  case ErrorCode::NotConfigured:
    return "NotConfigured";
  case ErrorCode::NotArmed:
    return "NotArmed";
  case ErrorCode::AlreadyRunning:
    return "AlreadyRunning";
  case ErrorCode::HardwareNotFound:
    return "HardwareNotFound";
  case ErrorCode::HardwareConnectionFailed:
    return "HardwareConnectionFailed";
  case ErrorCode::HardwareTimeout:
    return "HardwareTimeout";
  case ErrorCode::CommunicationError:
    return "CommunicationError";
  case ErrorCode::Timeout:
    return "Timeout";
  case ErrorCode::ConnectionLost:
    return "ConnectionLost";
  case ErrorCode::InternalError:
    return "InternalError";
  case ErrorCode::OutOfMemory:
    return "OutOfMemory";
  case ErrorCode::Unknown:
  default:
    return "Unknown";
  }
}

} // namespace DELILA

#endif // DELILA_CORE_ERROR_CODE_HPP
