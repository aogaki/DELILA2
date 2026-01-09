#ifndef DELILA_CORE_IOPERATOR_HPP
#define DELILA_CORE_IOPERATOR_HPP

#include "ComponentState.hpp"
#include "ComponentStatus.hpp"
#include "IComponent.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace DELILA {

/**
 * @brief State of an asynchronous job
 */
enum class JobState {
  Pending,   ///< Job is queued but not yet started
  Running,   ///< Job is currently executing
  Completed, ///< Job finished successfully
  Failed     ///< Job failed with an error
};

/**
 * @brief Status information for an asynchronous job
 */
struct JobStatus {
  std::string job_id;      ///< Unique job identifier
  JobState state;          ///< Current job state
  std::string error_message; ///< Error message if state is Failed

  std::chrono::system_clock::time_point created_at;   ///< When job was created
  std::chrono::system_clock::time_point completed_at; ///< When job finished

  JobStatus()
      : state(JobState::Pending),
        created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Interface for system operators
 *
 * Extends IComponent with system-wide control capabilities.
 * The operator manages multiple DataComponents and coordinates
 * their state transitions.
 *
 * Key responsibilities:
 * - Send commands to all components (Configure, Arm, Start, Stop, Reset)
 * - Monitor component status via heartbeats
 * - Coordinate synchronized start (two-phase commit)
 * - Handle graceful and emergency stops
 *
 * Async Pattern:
 *   All control methods return immediately with a job_id.
 *   Use GetJobStatus(job_id) to poll for completion.
 *
 * Example:
 *   auto job_id = operator->ConfigureAllAsync();
 *   while (operator->GetJobStatus(job_id).state == JobState::Running) {
 *       std::this_thread::sleep_for(100ms);
 *   }
 *   if (operator->GetJobStatus(job_id).state == JobState::Completed) {
 *       // All components configured successfully
 *   }
 */
class IOperator : public IComponent {
public:
  virtual ~IOperator() = default;

  // === Async Commands (return job_id) ===

  /**
   * @brief Configure all managed components
   * @return Job ID for tracking progress
   *
   * Sends Configure command to all components.
   * Waits for all to reach Configured state.
   */
  virtual std::string ConfigureAllAsync() = 0;

  /**
   * @brief Arm all managed components
   * @return Job ID for tracking progress
   *
   * Sends Arm command to all components.
   * Waits for all to reach Armed state.
   * Fails if any component fails to arm.
   */
  virtual std::string ArmAllAsync() = 0;

  /**
   * @brief Start all managed components
   * @param run_number Run number for this acquisition
   * @return Job ID for tracking progress
   *
   * Sends Start command to all components.
   * Components should be Armed before calling this.
   */
  virtual std::string StartAllAsync(uint32_t run_number) = 0;

  /**
   * @brief Stop all managed components
   * @param graceful true for graceful stop (flush data), false for emergency
   * @return Job ID for tracking progress
   *
   * Graceful: Stops sources first, waits for data flush, then stops sinks.
   * Emergency: Stops all components immediately.
   */
  virtual std::string StopAllAsync(bool graceful) = 0;

  /**
   * @brief Reset all managed components to Idle state
   * @return Job ID for tracking progress
   *
   * Clears configuration and recovers from Error state.
   */
  virtual std::string ResetAllAsync() = 0;

  // === Job Status ===

  /**
   * @brief Get status of an async job
   * @param job_id Job identifier returned by async methods
   * @return Current job status
   */
  virtual JobStatus GetJobStatus(const std::string &job_id) const = 0;

  // === Component Status ===

  /**
   * @brief Get status of all managed components
   * @return List of component statuses
   */
  virtual std::vector<ComponentStatus> GetAllComponentStatus() const = 0;

  /**
   * @brief Get status of a specific component
   * @param component_id Component identifier
   * @return Component status
   */
  virtual ComponentStatus
  GetComponentStatus(const std::string &component_id) const = 0;

  // === Component Management ===

  /**
   * @brief Get list of all managed component IDs
   * @return List of component identifiers
   */
  virtual std::vector<std::string> GetComponentIds() const = 0;

  /**
   * @brief Check if all components are in the specified state
   * @param state Target state to check
   * @return true if all components are in the specified state
   */
  virtual bool IsAllInState(ComponentState state) const = 0;
};

} // namespace DELILA

#endif // DELILA_CORE_IOPERATOR_HPP
