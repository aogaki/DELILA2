/**
 * @file TwoPhaseStartManager.hpp
 * @brief Network-level coordination for synchronized multi-node DAQ start
 *
 * This class provides network-level state coordination for two-phase start.
 * It does NOT replace or duplicate the digitizer's hardware commands
 * (ArmAcquisition, SwStartAcquisition). Instead, it coordinates when those
 * commands should be sent across multiple networked DAQ nodes.
 *
 * Workflow with multiple digitizers:
 *
 *   Controller                    Node 1                    Node 2
 *       |                           |                         |
 *       |------ Configure() ------->|                         |
 *       |------ Configure() --------------------------------->|
 *       |                           |                         |
 *       |<----- Configured OK ------|                         |
 *       |<----- Configured OK -------------------------------+|
 *       |                           |                         |
 *       |------- Arm() ------------>| (calls ArmAcquisition)  |
 *       |------- Arm() --------------------------------->| (calls ArmAcquisition)
 *       |                           |                         |
 *       |<------ Armed OK ----------|                         |
 *       |<------ Armed OK -----------------------------------|
 *       |                           |                         |
 *       | (all nodes armed)         |                         |
 *       |                           |                         |
 *       |------ Trigger() --------->| (calls SwStartAcquisition)
 *       |------ Trigger() ---------------------------------->| (same time)
 *       |                           |                         |
 *
 * The key benefit: All nodes are in Armed state before any node starts
 * running, eliminating the need for sleep-based synchronization.
 */

#pragma once

#include "ComponentState.hpp"

namespace DELILA {
namespace Net {

/**
 * @brief Tracks two-phase start coordination state
 *
 * This is a simple state machine for network coordination.
 * Each DAQ node has its own TwoPhaseStartManager instance.
 * A central controller coordinates state transitions across all nodes.
 */
class TwoPhaseStartManager {
public:
    /**
     * @brief Result of state transition operations
     */
    enum class Result {
        Success,       ///< Operation succeeded
        InvalidState,  ///< Operation not valid in current state
        NotArmed,      ///< Trigger attempted but not armed
        AlreadyArmed   ///< Arm attempted but already armed
    };

    /**
     * @brief Constructor - starts in Loaded state
     */
    TwoPhaseStartManager() : state_(ComponentState::Loaded) {}

    /**
     * @brief Get current state
     * @return Current ComponentState
     */
    ComponentState GetState() const { return state_; }

    /**
     * @brief Mark as configured (after digitizer Configure() succeeds)
     * @return Result::Success if transition succeeded
     *
     * This does NOT call digitizer Configure() - it only tracks state.
     * The calling code should call digitizer.Configure() first.
     */
    Result Configure()
    {
        if (state_ != ComponentState::Loaded) {
            return Result::InvalidState;
        }
        state_ = ComponentState::Configured;
        return Result::Success;
    }

    /**
     * @brief Mark as armed (after digitizer ArmAcquisition succeeds)
     * @return Result::Success if transition succeeded
     *
     * This does NOT call ArmAcquisition - it only tracks state.
     * The calling code should call the hardware arm command first.
     */
    Result Arm()
    {
        if (state_ == ComponentState::Armed) {
            return Result::AlreadyArmed;
        }
        if (state_ != ComponentState::Configured) {
            return Result::InvalidState;
        }
        state_ = ComponentState::Armed;
        return Result::Success;
    }

    /**
     * @brief Mark as running (when trigger command is received)
     * @return Result::Success if transition succeeded
     *
     * This is called when the central controller sends "start" to all nodes.
     * Only valid when all nodes are armed.
     */
    Result Trigger()
    {
        if (state_ != ComponentState::Armed) {
            return Result::NotArmed;
        }
        state_ = ComponentState::Running;
        return Result::Success;
    }

    /**
     * @brief Stop and return to Loaded state
     * @return Result::Success if transition succeeded
     */
    Result Stop()
    {
        if (state_ != ComponentState::Running && state_ != ComponentState::Armed) {
            return Result::InvalidState;
        }
        state_ = ComponentState::Loaded;
        return Result::Success;
    }

    /**
     * @brief Reset to initial state from any state
     * @return Result::Success always
     */
    Result Reset()
    {
        state_ = ComponentState::Loaded;
        return Result::Success;
    }

    /**
     * @brief Check if armed and ready for trigger
     * @return true if in Armed state
     */
    bool IsArmed() const { return state_ == ComponentState::Armed; }

    /**
     * @brief Check if running
     * @return true if in Running state
     */
    bool IsRunning() const { return state_ == ComponentState::Running; }

private:
    ComponentState state_;
};

}  // namespace Net
}  // namespace DELILA
