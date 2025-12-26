#ifndef DELILA_CORE_COMPONENT_STATE_HPP
#define DELILA_CORE_COMPONENT_STATE_HPP

#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief State machine for DAQ components
 *
 * Represents the lifecycle states of components like Digitizer, Sink, Monitor.
 * State transitions:
 *   Idle -> Configured -> Armed -> Running
 *   Any state -> Error
 *   Error -> Idle (via Reset)
 */
enum class ComponentState : uint8_t {
  Idle = 0,       ///< Initial state, not configured
  Configured = 1, ///< Configuration loaded and validated
  Armed = 2,      ///< Ready to start acquisition
  Running = 3,    ///< Actively acquiring data
  Error = 4       ///< Error state, requires reset
};

/**
 * @brief Convert ComponentState to string for logging/debugging
 */
inline std::string ComponentStateToString(ComponentState state) {
  switch (state) {
  case ComponentState::Idle:
    return "Idle";
  case ComponentState::Configured:
    return "Configured";
  case ComponentState::Armed:
    return "Armed";
  case ComponentState::Running:
    return "Running";
  case ComponentState::Error:
    return "Error";
  default:
    return "Unknown";
  }
}

} // namespace DELILA

#endif // DELILA_CORE_COMPONENT_STATE_HPP
