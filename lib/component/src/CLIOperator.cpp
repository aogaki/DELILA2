#include "CLIOperator.hpp"

#include <ZMQTransport.hpp>
#include <delila/core/ErrorCode.hpp>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace DELILA {

CLIOperator::CLIOperator() = default;

CLIOperator::~CLIOperator() { Shutdown(); }

// === IComponent interface ===

bool CLIOperator::Initialize(const std::string & /*config_path*/) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Idle) {
    return false;
  }

  // TODO: Load configuration from file if needed

  fState = ComponentState::Configured;
  return true;
}

void CLIOperator::Run() {
  // Main loop - wait for shutdown
  while (!fShutdownRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fHeartbeatCounter++;
  }
}

void CLIOperator::Shutdown() {
  fShutdownRequested = true;

  // Clear jobs
  {
    std::lock_guard<std::mutex> lock(fJobsMutex);
    fJobs.clear();
  }

  // Clear components
  {
    std::lock_guard<std::mutex> lock(fComponentsMutex);
    fComponents.clear();
  }

  fState = ComponentState::Idle;
}

ComponentState CLIOperator::GetState() const { return fState.load(); }

std::string CLIOperator::GetComponentId() const { return fComponentId; }

ComponentStatus CLIOperator::GetStatus() const {
  ComponentStatus status;
  status.component_id = fComponentId;
  status.state = fState.load();
  status.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  status.run_number = 0;  // Operator doesn't have a run number
  status.metrics.events_processed = 0;
  status.metrics.bytes_transferred = 0;
  status.error_message = fErrorMessage;
  status.heartbeat_counter = fHeartbeatCounter.load();
  return status;
}

// === IOperator interface - Async Commands ===

std::string CLIOperator::ConfigureAllAsync() {
  auto job_id = GenerateJobId();

  ExecuteJob(job_id, [this]() {
    std::lock_guard<std::mutex> lock(fComponentsMutex);

    for (const auto &component : fComponents) {
      Command cmd(CommandType::Configure);

      auto response = SendCommandToComponent(component, cmd);
      if (response.success) {
        fComponentStates[component.component_id] = response.current_state;
      } else {
        throw std::runtime_error("Failed to configure " + component.component_id +
                                 ": " + response.message);
      }
    }
  });

  return job_id;
}

std::string CLIOperator::ArmAllAsync() {
  auto job_id = GenerateJobId();

  ExecuteJob(job_id, [this]() {
    std::lock_guard<std::mutex> lock(fComponentsMutex);

    for (const auto &component : fComponents) {
      Command cmd(CommandType::Arm);

      auto response = SendCommandToComponent(component, cmd);
      if (response.success) {
        fComponentStates[component.component_id] = response.current_state;
      } else {
        throw std::runtime_error("Failed to arm " + component.component_id +
                                 ": " + response.message);
      }
    }
  });

  return job_id;
}

std::string CLIOperator::StartAllAsync(uint32_t run_number) {
  auto job_id = GenerateJobId();

  ExecuteJob(job_id, [this, run_number]() {
    std::lock_guard<std::mutex> lock(fComponentsMutex);

    for (const auto &component : fComponents) {
      Command cmd(CommandType::Start);
      cmd.run_number = run_number;

      auto response = SendCommandToComponent(component, cmd);
      if (response.success) {
        fComponentStates[component.component_id] = response.current_state;
      } else {
        throw std::runtime_error("Failed to start " + component.component_id +
                                 ": " + response.message);
      }
    }
  });

  return job_id;
}

std::string CLIOperator::StopAllAsync(bool graceful) {
  auto job_id = GenerateJobId();

  ExecuteJob(job_id, [this, graceful]() {
    std::lock_guard<std::mutex> lock(fComponentsMutex);

    for (const auto &component : fComponents) {
      Command cmd(CommandType::Stop);
      cmd.graceful = graceful;

      auto response = SendCommandToComponent(component, cmd);
      if (response.success) {
        fComponentStates[component.component_id] = response.current_state;
      } else {
        throw std::runtime_error("Failed to stop " + component.component_id +
                                 ": " + response.message);
      }
    }
  });

  return job_id;
}

std::string CLIOperator::ResetAllAsync() {
  auto job_id = GenerateJobId();

  ExecuteJob(job_id, [this]() {
    std::lock_guard<std::mutex> lock(fComponentsMutex);

    for (const auto &component : fComponents) {
      Command cmd(CommandType::Reset);

      auto response = SendCommandToComponent(component, cmd);
      if (response.success) {
        fComponentStates[component.component_id] = response.current_state;
      } else {
        throw std::runtime_error("Failed to reset " + component.component_id +
                                 ": " + response.message);
      }
    }
  });

  return job_id;
}

// === IOperator interface - Job Status ===

JobStatus CLIOperator::GetJobStatus(const std::string &job_id) const {
  std::lock_guard<std::mutex> lock(fJobsMutex);

  auto it = fJobs.find(job_id);
  if (it != fJobs.end()) {
    return it->second;
  }

  // Return empty status for unknown job
  return JobStatus{};
}

// === IOperator interface - Component Status ===

std::vector<ComponentStatus> CLIOperator::GetAllComponentStatus() const {
  std::lock_guard<std::mutex> lock(fComponentsMutex);

  std::vector<ComponentStatus> statuses;
  for (const auto &component : fComponents) {
    ComponentStatus status;
    status.component_id = component.component_id;
    auto it = fComponentStates.find(component.component_id);
    if (it != fComponentStates.end()) {
      status.state = it->second;
    } else {
      status.state = ComponentState::Idle;
    }
    statuses.push_back(status);
  }
  return statuses;
}

ComponentStatus
CLIOperator::GetComponentStatus(const std::string &component_id) const {
  std::lock_guard<std::mutex> lock(fComponentsMutex);

  ComponentStatus status;
  // Check if component is registered
  bool found = false;
  for (const auto &comp : fComponents) {
    if (comp.component_id == component_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    // Return empty status for unknown component
    return status;
  }
  status.component_id = component_id;
  auto it = fComponentStates.find(component_id);
  if (it != fComponentStates.end()) {
    status.state = it->second;
  } else {
    status.state = ComponentState::Idle;
  }
  return status;
}

// === IOperator interface - Component Management ===

std::vector<std::string> CLIOperator::GetComponentIds() const {
  std::lock_guard<std::mutex> lock(fComponentsMutex);

  std::vector<std::string> ids;
  for (const auto &component : fComponents) {
    ids.push_back(component.component_id);
  }
  return ids;
}

bool CLIOperator::IsAllInState(ComponentState state) const {
  std::lock_guard<std::mutex> lock(fComponentsMutex);

  // With no components, vacuously true
  if (fComponents.empty()) {
    return true;
  }

  for (const auto &component : fComponents) {
    auto it = fComponentStates.find(component.component_id);
    if (it == fComponentStates.end() || it->second != state) {
      return false;
    }
  }
  return true;
}

// === Additional methods ===

void CLIOperator::SetComponentId(const std::string &id) { fComponentId = id; }

void CLIOperator::RegisterComponent(const ComponentAddress &address) {
  std::lock_guard<std::mutex> lock(fComponentsMutex);
  fComponents.push_back(address);
}

void CLIOperator::UnregisterComponent(const std::string &component_id) {
  std::lock_guard<std::mutex> lock(fComponentsMutex);

  fComponents.erase(
      std::remove_if(fComponents.begin(), fComponents.end(),
                     [&component_id](const ComponentAddress &addr) {
                       return addr.component_id == component_id;
                     }),
      fComponents.end());
}

// === Testing utilities ===

void CLIOperator::ForceError(const std::string &message) {
  fErrorMessage = message;
  fState = ComponentState::Error;
}

void CLIOperator::Reset() {
  fErrorMessage.clear();
  fState = ComponentState::Idle;
}

// === IComponent callbacks ===

bool CLIOperator::OnConfigure(const nlohmann::json & /*config*/) {
  // Already handled in Initialize
  return true;
}

bool CLIOperator::OnArm() {
  // Operator doesn't need to arm itself
  return true;
}

bool CLIOperator::OnStart(uint32_t /*run_number*/) {
  // Operator doesn't run like a data component
  return true;
}

bool CLIOperator::OnStop(bool /*graceful*/) {
  // Operator doesn't stop like a data component
  return true;
}

void CLIOperator::OnReset() {
  fErrorMessage.clear();
  fState = ComponentState::Idle;
}

// === Job management ===

std::string CLIOperator::GenerateJobId() {
  uint64_t counter = fJobCounter.fetch_add(1);
  std::ostringstream oss;
  oss << "job_" << std::setfill('0') << std::setw(6) << counter;
  return oss.str();
}

void CLIOperator::ExecuteJob(const std::string &job_id,
                              std::function<void()> task) {
  // Create job entry
  {
    std::lock_guard<std::mutex> lock(fJobsMutex);
    JobStatus status;
    status.job_id = job_id;
    status.state = JobState::Running;
    status.created_at = std::chrono::system_clock::now();
    fJobs[job_id] = status;
  }

  // Execute task asynchronously
  std::thread([this, job_id, task]() {
    try {
      task();

      // Mark job as completed
      {
        std::lock_guard<std::mutex> lock(fJobsMutex);
        auto it = fJobs.find(job_id);
        if (it != fJobs.end()) {
          it->second.state = JobState::Completed;
          it->second.completed_at = std::chrono::system_clock::now();
        }
      }
    } catch (const std::exception &e) {
      // Mark job as failed
      {
        std::lock_guard<std::mutex> lock(fJobsMutex);
        auto it = fJobs.find(job_id);
        if (it != fJobs.end()) {
          it->second.state = JobState::Failed;
          it->second.error_message = e.what();
          it->second.completed_at = std::chrono::system_clock::now();
        }
      }
    } catch (...) {
      // Mark job as failed for unknown exceptions
      {
        std::lock_guard<std::mutex> lock(fJobsMutex);
        auto it = fJobs.find(job_id);
        if (it != fJobs.end()) {
          it->second.state = JobState::Failed;
          it->second.error_message = "Unknown exception";
          it->second.completed_at = std::chrono::system_clock::now();
        }
      }
    }
  }).detach();
}

// === Command sending ===

CommandResponse CLIOperator::SendCommandToComponent(const ComponentAddress &component,
                                                     const Command &cmd) {
  CommandResponse response;
  response.success = false;
  response.error_code = ErrorCode::CommunicationError;

  // Create a temporary transport for this command
  auto transport = std::make_unique<Net::ZMQTransport>();
  Net::TransportConfig config;
  config.command_address = component.command_address;
  config.bind_command = false;  // Connect to component's REP socket
  // Disable data and status sockets
  config.data_address = "";
  config.status_address = "";

  if (!transport->Configure(config)) {
    response.message = "Failed to configure transport";
    return response;
  }

  if (!transport->Connect()) {
    response.message = "Failed to connect to component";
    return response;
  }

  // Send command and wait for response
  auto receivedResponse = transport->SendCommand(cmd);
  if (receivedResponse) {
    response = *receivedResponse;
  } else {
    response.message = "No response from component";
  }

  transport->Disconnect();
  return response;
}

}  // namespace DELILA
