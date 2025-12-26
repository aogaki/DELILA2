/**
 * @file ComponentState.hpp
 * @brief Component state machine for DELILA2 components
 *
 * State machine for DAQ component lifecycle:
 *   Loaded -> Configured -> Armed -> Running <-> Paused
 *                                      |
 *                                      v
 *                                   Loaded (stop)
 *
 * The Armed state is key for 2-phase start:
 * 1. Arm: All sources prepare hardware (digitizers in armed state)
 * 2. Trigger: Master sends software start, hardware cascades
 */

#pragma once

#include <cstdint>
#include <string>

namespace DELILA {
namespace Net {

/**
 * @brief Component lifecycle states
 */
enum class ComponentState : uint8_t {
    Loaded,      ///< Initial state after loading
    Configured,  ///< Configuration applied
    Armed,       ///< Ready to start (hardware prepared)
    Running,     ///< Data acquisition in progress
    Paused,      ///< Temporarily paused
    Error        ///< Error state
};

/**
 * @brief Convert ComponentState to string
 */
inline std::string ComponentStateToString(ComponentState state)
{
    switch (state) {
        case ComponentState::Loaded:
            return "Loaded";
        case ComponentState::Configured:
            return "Configured";
        case ComponentState::Armed:
            return "Armed";
        case ComponentState::Running:
            return "Running";
        case ComponentState::Paused:
            return "Paused";
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
 * - Loaded -> Configured
 * - Configured -> Armed
 * - Armed -> Running
 * - Running -> Paused
 * - Paused -> Running
 * - Any -> Loaded (reset/stop)
 * - Any -> Error
 */
inline bool IsValidTransition(ComponentState from, ComponentState to)
{
    // Same state is not a transition
    if (from == to) {
        return false;
    }

    // Any state can go to Loaded (reset) or Error
    if (to == ComponentState::Loaded || to == ComponentState::Error) {
        return true;
    }

    // Check specific valid transitions
    switch (from) {
        case ComponentState::Loaded:
            return to == ComponentState::Configured;

        case ComponentState::Configured:
            return to == ComponentState::Armed;

        case ComponentState::Armed:
            return to == ComponentState::Running;

        case ComponentState::Running:
            return to == ComponentState::Paused;

        case ComponentState::Paused:
            return to == ComponentState::Running;

        case ComponentState::Error:
            return false;  // Error can only go to Loaded

        default:
            return false;
    }
}

}  // namespace Net
}  // namespace DELILA
