/**
 * @file CLIOperator.hpp
 * @brief Command-line interface operator for DAQ system control
 *
 * CLIOperator implements IOperator and provides:
 * - Management of multiple DataComponents
 * - Async command execution (Configure, Arm, Start, Stop, Reset)
 * - Component status monitoring
 * - Job status tracking
 */

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "delila/core/Command.hpp"
#include "delila/core/CommandResponse.hpp"
#include "delila/core/ComponentState.hpp"
#include "delila/core/ComponentStatus.hpp"
#include "delila/core/IOperator.hpp"
#include "delila/core/OperatorConfig.hpp"

namespace DELILA {

namespace Net {
class ZMQTransport;
}  // namespace Net

/**
 * @brief CLI-based operator for controlling DAQ components
 *
 * This component:
 * - Manages multiple DataComponents via ZMQ commands
 * - Provides async job execution with status tracking
 * - Monitors component health via heartbeats
 *
 * State transitions follow IComponent standard:
 *   Idle -> Configured (after Initialize)
 */
class CLIOperator : public IOperator {
public:
  CLIOperator();
  ~CLIOperator() override;

  // Disable copy
  CLIOperator(const CLIOperator &) = delete;
  CLIOperator &operator=(const CLIOperator &) = delete;

  // === IComponent interface ===
  bool Initialize(const std::string &config_path) override;
  void Run() override;
  void Shutdown() override;
  ComponentState GetState() const override;
  std::string GetComponentId() const override;
  ComponentStatus GetStatus() const override;

  // === IOperator interface - Async Commands ===
  std::string ConfigureAllAsync() override;
  std::string ArmAllAsync() override;
  std::string StartAllAsync(uint32_t run_number) override;
  std::string StopAllAsync(bool graceful) override;
  std::string ResetAllAsync() override;

  // === IOperator interface - Job Status ===
  JobStatus GetJobStatus(const std::string &job_id) const override;

  // === IOperator interface - Component Status ===
  std::vector<ComponentStatus> GetAllComponentStatus() const override;
  ComponentStatus GetComponentStatus(const std::string &component_id) const override;

  // === IOperator interface - Component Management ===
  std::vector<std::string> GetComponentIds() const override;
  bool IsAllInState(ComponentState state) const override;

  // === Additional methods ===
  void SetComponentId(const std::string &id);
  void RegisterComponent(const ComponentAddress &address);
  void UnregisterComponent(const std::string &component_id);

  // === Testing utilities ===
  void ForceError(const std::string &message);
  void Reset();

protected:
  // === IComponent callbacks ===
  bool OnConfigure(const nlohmann::json &config) override;
  bool OnArm() override;
  bool OnStart(uint32_t run_number) override;
  bool OnStop(bool graceful) override;
  void OnReset() override;

private:
  // === Job management ===
  std::string GenerateJobId();
  void ExecuteJob(const std::string &job_id, std::function<void()> task);

  // === Command sending ===
  CommandResponse SendCommandToComponent(const ComponentAddress &component,
                                          const Command &cmd);

  // === State ===
  std::atomic<ComponentState> fState{ComponentState::Idle};
  mutable std::mutex fStateMutex;
  std::string fComponentId;
  std::string fErrorMessage;

  // === Component management ===
  std::vector<ComponentAddress> fComponents;
  mutable std::mutex fComponentsMutex;
  std::map<std::string, ComponentState> fComponentStates;

  // === Job tracking ===
  std::map<std::string, JobStatus> fJobs;
  mutable std::mutex fJobsMutex;
  std::atomic<uint64_t> fJobCounter{0};

  // === Status ===
  std::atomic<uint64_t> fHeartbeatCounter{0};

  // === Threads ===
  std::atomic<bool> fShutdownRequested{false};
};

}  // namespace DELILA
