#ifndef DELILA_CORE_COMPONENT_STATE_HPP
#define DELILA_CORE_COMPONENT_STATE_HPP

#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief State machine for DAQ components
 *
 * Represents the lifecycle states of components like DigitizerSource, FileWriter, etc.
 *
 * State transitions:
 *   Idle -> Configuring -> Configured -> Arming -> Armed -> Starting -> Running -> Stopping -> Configured
 *   Any state -> Error
 *   Error -> Idle (via Reset)
 *
 * Transitional states (Configuring, Arming, Starting, Stopping) indicate
 * that an asynchronous operation is in progress.
 */
enum class ComponentState : uint8_t {
  Idle = 0,        ///< Initial state, not configured
  Configuring = 1, ///< Configuration in progress
  Configured = 2,  ///< Configuration loaded and validated
  Arming = 3,      ///< Arm operation in progress
  Armed = 4,       ///< Ready to start acquisition (hardware prepared)
  Starting = 5,    ///< Start operation in progress
  Running = 6,     ///< Actively acquiring data
  Stopping = 7,    ///< Stop operation in progress
  Error = 8        ///< Error state, requires reset
};

/**
 * @brief Convert ComponentState to string for logging/debugging
 */
inline std::string ComponentStateToString(ComponentState state) {
  switch (state) {
  case ComponentState::Idle:
    return "Idle";
  case ComponentState::Configuring:
    return "Configuring";
  case ComponentState::Configured:
    return "Configured";
  case ComponentState::Arming:
    return "Arming";
  case ComponentState::Armed:
    return "Armed";
  case ComponentState::Starting:
    return "Starting";
  case ComponentState::Running:
    return "Running";
  case ComponentState::Stopping:
    return "Stopping";
  case ComponentState::Error:
    return "Error";
  default:
    return "Unknown";
  }
}

/**
 * @brief Check if a state transition is valid
 * @param from Current state
 * @param to Target state
 * @return true if transition is allowed
 *
 * Valid transitions:
 * - Idle -> Configuring
 * - Configuring -> Configured
 * - Configured -> Arming
 * - Arming -> Armed
 * - Armed -> Starting
 * - Starting -> Running
 * - Running -> Stopping
 * - Stopping -> Configured
 * - Any -> Idle (reset/stop)
 * - Any -> Error
 */
inline bool IsValidTransition(ComponentState from, ComponentState to) {
  // Same state is not a transition
  if (from == to) {
    return false;
  }

  // Any state can go to Idle (reset) or Error
  if (to == ComponentState::Idle || to == ComponentState::Error) {
    return true;
  }

  // Check specific valid transitions
  switch (from) {
  case ComponentState::Idle:
    return to == ComponentState::Configuring;

  case ComponentState::Configuring:
    return to == ComponentState::Configured;

  case ComponentState::Configured:
    return to == ComponentState::Arming;

  case ComponentState::Arming:
    return to == ComponentState::Armed;

  case ComponentState::Armed:
    return to == ComponentState::Starting;

  case ComponentState::Starting:
    return to == ComponentState::Running;

  case ComponentState::Running:
    return to == ComponentState::Stopping;

  case ComponentState::Stopping:
    return to == ComponentState::Configured;

  case ComponentState::Error:
    return false; // Error can only go to Idle (handled above)

  default:
    return false;
  }
}

} // namespace DELILA

#endif // DELILA_CORE_COMPONENT_STATE_HPP
