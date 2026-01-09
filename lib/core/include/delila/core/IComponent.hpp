#ifndef DELILA_CORE_ICOMPONENT_HPP
#define DELILA_CORE_ICOMPONENT_HPP

#include "ComponentState.hpp"
#include "ComponentStatus.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace DELILA {

/**
 * @brief Base interface for all DAQ components
 *
 * Defines the common lifecycle for all DAQ components:
 * - DigitizerSource: Hardware interface for data acquisition
 * - FileWriter: Data storage
 * - TimeSortMerger: Multi-source data merging
 *
 * Lifecycle:
 *   1. Construct component
 *   2. Initialize(config_path) - load configuration
 *   3. Run() - start main loop (blocking)
 *   4. Shutdown() - cleanup
 *
 * State transitions (triggered by operator commands):
 *   Idle -> Configuring -> Configured -> Arming -> Armed -> Starting -> Running
 *   Running -> Stopping -> Configured
 *   Any -> Error
 *   Error -> Idle (via Reset)
 */
class IComponent {
public:
  virtual ~IComponent() = default;

  // === Lifecycle (called by main/framework) ===

  /**
   * @brief Initialize component with configuration file
   * @param config_path Path to JSON configuration file
   * @return true if initialization succeeded
   *
   * Loads configuration and prepares the component.
   * After this call, the component is ready to receive commands.
   */
  virtual bool Initialize(const std::string &config_path) = 0;

  /**
   * @brief Run the component main loop
   *
   * Starts all threads and enters the main loop.
   * This method blocks until Shutdown() is called.
   * Handles command reception and state transitions.
   */
  virtual void Run() = 0;

  /**
   * @brief Shutdown the component
   *
   * Stops all threads and releases resources.
   * Called when the component should terminate.
   */
  virtual void Shutdown() = 0;

  // === State Query ===

  /**
   * @brief Get current component state
   */
  virtual ComponentState GetState() const = 0;

  /**
   * @brief Get unique component identifier
   *
   * Format: "type_hostname_index" (e.g., "digitizer_daq01_0")
   */
  virtual std::string GetComponentId() const = 0;

  /**
   * @brief Get current component status with metrics
   */
  virtual ComponentStatus GetStatus() const = 0;

protected:
  // === Callbacks (implemented by derived classes) ===

  /**
   * @brief Called when Configure command is received
   * @param config JSON configuration object
   * @return true if configuration succeeded
   *
   * Transitions: Idle -> Configuring -> Configured
   */
  virtual bool OnConfigure(const nlohmann::json &config) = 0;

  /**
   * @brief Called when Arm command is received
   * @return true if arm succeeded
   *
   * Prepares hardware for synchronized start.
   * Transitions: Configured -> Arming -> Armed
   */
  virtual bool OnArm() = 0;

  /**
   * @brief Called when Start command is received
   * @param run_number Run number for this acquisition
   * @return true if start succeeded
   *
   * Transitions: Armed -> Starting -> Running
   */
  virtual bool OnStart(uint32_t run_number) = 0;

  /**
   * @brief Called when Stop command is received
   * @param graceful true for graceful stop, false for emergency stop
   * @return true if stop succeeded
   *
   * Transitions: Running -> Stopping -> Configured
   */
  virtual bool OnStop(bool graceful) = 0;

  /**
   * @brief Called when Reset command is received
   *
   * Resets component to Idle state.
   * Clears configuration and recovers from Error state.
   */
  virtual void OnReset() = 0;
};

} // namespace DELILA

#endif // DELILA_CORE_ICOMPONENT_HPP
