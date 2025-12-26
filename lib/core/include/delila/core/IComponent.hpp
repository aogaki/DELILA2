#ifndef DELILA_CORE_ICOMPONENT_HPP
#define DELILA_CORE_ICOMPONENT_HPP

#include "ComponentState.hpp"
#include <string>

namespace DELILA {

/**
 * @brief Interface for DAQ components
 *
 * Defines the common lifecycle for all DAQ components:
 * - Digitizer: Hardware interface for data acquisition
 * - Sink: Data receiver and storage
 * - Monitor: Real-time data visualization
 *
 * State transitions:
 *   Idle --Configure()--> Configured
 *   Configured --Start()--> Armed
 *   Armed --internal--> Running
 *   Running --Stop()--> Configured
 *   Any --Reset()--> Idle
 *   Any --error--> Error
 */
class IComponent {
public:
  virtual ~IComponent() = default;

  // Lifecycle methods
  /**
   * @brief Load and validate configuration
   * @param config_path Path to configuration file
   * @return true if configuration is valid and loaded
   *
   * Transitions: Idle -> Configured
   */
  virtual bool Configure(const std::string &config_path) = 0;

  /**
   * @brief Start data acquisition
   * @return true if started successfully
   *
   * Transitions: Configured -> Armed -> Running
   */
  virtual bool Start() = 0;

  /**
   * @brief Stop data acquisition
   * @return true if stopped successfully
   *
   * Transitions: Running -> Configured
   */
  virtual bool Stop() = 0;

  /**
   * @brief Reset component to Idle state
   *
   * Clears all configuration and recovers from Error state.
   * Transitions: Any -> Idle
   */
  virtual void Reset() = 0;

  // State query
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
};

} // namespace DELILA

#endif // DELILA_CORE_ICOMPONENT_HPP
