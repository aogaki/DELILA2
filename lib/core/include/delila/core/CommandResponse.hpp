#ifndef DELILA_CORE_COMMAND_RESPONSE_HPP
#define DELILA_CORE_COMMAND_RESPONSE_HPP

#include "ComponentState.hpp"
#include "ErrorCode.hpp"
#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief Response structure for commands
 */
struct CommandResponse {
  uint32_t request_id;         ///< Request ID from original command
  bool success;                ///< True if command succeeded
  ErrorCode error_code;        ///< Error code if !success
  ComponentState current_state; ///< Current component state
  std::string message;         ///< Human-readable message or error description
  std::string payload;         ///< Additional response data (JSON)

  CommandResponse()
      : request_id(0), success(true), error_code(ErrorCode::Success),
        current_state(ComponentState::Idle) {}

  CommandResponse(uint32_t id, bool ok,
                  ComponentState state = ComponentState::Idle)
      : request_id(id), success(ok),
        error_code(ok ? ErrorCode::Success : ErrorCode::Unknown),
        current_state(state) {}

  /// Create a success response
  static CommandResponse Success(uint32_t id, ComponentState state,
                                  const std::string &msg = "") {
    CommandResponse resp;
    resp.request_id = id;
    resp.success = true;
    resp.error_code = ErrorCode::Success;
    resp.current_state = state;
    resp.message = msg;
    return resp;
  }

  /// Create an error response
  static CommandResponse Error(uint32_t id, ErrorCode code,
                                const std::string &msg,
                                ComponentState state = ComponentState::Error) {
    CommandResponse resp;
    resp.request_id = id;
    resp.success = false;
    resp.error_code = code;
    resp.current_state = state;
    resp.message = msg;
    return resp;
  }
};

} // namespace DELILA

#endif // DELILA_CORE_COMMAND_RESPONSE_HPP
