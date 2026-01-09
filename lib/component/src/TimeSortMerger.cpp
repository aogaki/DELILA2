#include "TimeSortMerger.hpp"

#include <DataProcessor.hpp>
#include <EOSTracker.hpp>
#include <ZMQTransport.hpp>
#include <delila/core/CommandResponse.hpp>
#include <delila/core/ErrorCode.hpp>

#include <chrono>
#include <iostream>

namespace DELILA {

TimeSortMerger::TimeSortMerger()
    : fDataProcessor(std::make_unique<Net::DataProcessor>()),
      fEOSTracker(std::make_unique<Net::EOSTracker>()) {}

TimeSortMerger::~TimeSortMerger() { Shutdown(); }

// === IComponent interface ===

bool TimeSortMerger::Initialize(const std::string &config_path) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Idle) {
    return false;
  }

  // Validate: must have at least one input and one output
  if (fInputAddresses.empty()) {
    fErrorMessage = "No input addresses configured";
    return false;
  }

  if (fOutputAddresses.empty()) {
    fErrorMessage = "No output addresses configured";
    return false;
  }

  // Load configuration from file if provided
  if (!config_path.empty()) {
    // TODO: Load configuration from file
  }

  // Create input transports
  fInputTransports.clear();
  for (size_t i = 0; i < fInputAddresses.size(); ++i) {
    auto transport = std::make_unique<Net::ZMQTransport>();
    Net::TransportConfig transportConfig;
    transportConfig.data_address = fInputAddresses[i];
    transportConfig.bind_data = false;  // Connect to upstream
    transportConfig.data_pattern = "PULL";
    // Disable status and command sockets
    transportConfig.status_address = transportConfig.data_address;
    transportConfig.command_address = "";

    if (!transport->Configure(transportConfig)) {
      fErrorMessage = "Failed to configure input transport " + std::to_string(i);
      fState = ComponentState::Error;
      return false;
    }
    fInputTransports.push_back(std::move(transport));
  }

  // Create output transport
  fOutputTransport = std::make_unique<Net::ZMQTransport>();
  Net::TransportConfig outputConfig;
  outputConfig.data_address = fOutputAddresses[0];
  outputConfig.bind_data = true;  // Bind for downstream
  outputConfig.data_pattern = "PUSH";
  outputConfig.status_address = outputConfig.data_address;
  outputConfig.command_address = "";

  if (!fOutputTransport->Configure(outputConfig)) {
    fErrorMessage = "Failed to configure output transport";
    fState = ComponentState::Error;
    return false;
  }

  fState = ComponentState::Configured;
  return true;
}

void TimeSortMerger::Run() {
  // Main loop - wait for shutdown
  while (!fShutdownRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void TimeSortMerger::Shutdown() {
  fShutdownRequested = true;
  fRunning = false;

  // Stop command listener first
  StopCommandListener();

  // Stop worker threads
  for (auto &thread : fReceivingThreads) {
    if (thread && thread->joinable()) {
      thread->join();
    }
  }
  fReceivingThreads.clear();

  if (fMergingThread && fMergingThread->joinable()) {
    fMergingThread->join();
  }
  if (fSendingThread && fSendingThread->joinable()) {
    fSendingThread->join();
  }

  // Disconnect transports
  for (auto &transport : fInputTransports) {
    if (transport) {
      transport->Disconnect();
    }
  }
  fInputTransports.clear();

  if (fOutputTransport) {
    fOutputTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

ComponentState TimeSortMerger::GetState() const { return fState.load(); }

std::string TimeSortMerger::GetComponentId() const { return fComponentId; }

ComponentStatus TimeSortMerger::GetStatus() const {
  ComponentStatus status;
  status.component_id = fComponentId;
  status.state = fState.load();
  status.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  status.run_number = fRunNumber.load();
  status.metrics.events_processed = fEventsProcessed.load();
  status.metrics.bytes_transferred = fBytesTransferred.load();
  status.error_message = fErrorMessage;
  status.heartbeat_counter = fHeartbeatCounter.load();
  return status;
}

// === IDataComponent interface ===

void TimeSortMerger::SetInputAddresses(
    const std::vector<std::string> &addresses) {
  fInputAddresses = addresses;
}

void TimeSortMerger::SetOutputAddresses(
    const std::vector<std::string> &addresses) {
  fOutputAddresses = addresses;
}

std::vector<std::string> TimeSortMerger::GetInputAddresses() const {
  return fInputAddresses;
}

std::vector<std::string> TimeSortMerger::GetOutputAddresses() const {
  return fOutputAddresses;
}

// === Public control methods ===

bool TimeSortMerger::Arm() { return OnArm(); }

bool TimeSortMerger::Start(uint32_t run_number) { return OnStart(run_number); }

bool TimeSortMerger::Stop(bool graceful) { return OnStop(graceful); }

void TimeSortMerger::Reset() { OnReset(); }

// === Configuration ===

void TimeSortMerger::SetComponentId(const std::string &id) { fComponentId = id; }

void TimeSortMerger::SetSortWindowNs(uint64_t window_ns) {
  fSortWindowNs = window_ns;
}

uint64_t TimeSortMerger::GetSortWindowNs() const { return fSortWindowNs; }

size_t TimeSortMerger::GetInputCount() const { return fInputAddresses.size(); }

// === Testing utilities ===

void TimeSortMerger::ForceError(const std::string &message) {
  fErrorMessage = message;
  fState = ComponentState::Error;
}

// === IComponent callbacks ===

bool TimeSortMerger::OnConfigure(const nlohmann::json & /*config*/) {
  // Already handled in Initialize
  return true;
}

bool TimeSortMerger::OnArm() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Configured) {
    return false;
  }

  // Connect all input transports
  for (size_t i = 0; i < fInputTransports.size(); ++i) {
    auto &transport = fInputTransports[i];
    if (transport && !transport->IsConnected()) {
      if (!transport->Connect()) {
        fErrorMessage = "Failed to connect input transport " + std::to_string(i);
        fState = ComponentState::Error;
        return false;
      }
    }
  }

  // Connect output transport
  if (fOutputTransport && !fOutputTransport->IsConnected()) {
    if (!fOutputTransport->Connect()) {
      fErrorMessage = "Failed to connect output transport";
      fState = ComponentState::Error;
      return false;
    }
  }

  fState = ComponentState::Armed;
  return true;
}

bool TimeSortMerger::OnStart(uint32_t run_number) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Armed) {
    return false;
  }

  fRunNumber = run_number;
  fEventsProcessed = 0;
  fBytesTransferred = 0;

  // Reset EOS tracker and register sources
  fEOSTracker->Reset();
  for (size_t i = 0; i < fInputAddresses.size(); ++i) {
    fEOSTracker->RegisterSource("input_" + std::to_string(i));
  }

  fRunning = true;

  // Start receiving threads (one per input)
  fReceivingThreads.clear();
  for (size_t i = 0; i < fInputTransports.size(); ++i) {
    fReceivingThreads.push_back(
        std::make_unique<std::thread>(&TimeSortMerger::ReceivingLoop, this, i));
  }

  // TODO: Start merging and sending threads when sorting is implemented
  // fMergingThread = std::make_unique<std::thread>(&TimeSortMerger::MergingLoop, this);
  // fSendingThread = std::make_unique<std::thread>(&TimeSortMerger::SendingLoop, this);

  fState = ComponentState::Running;
  return true;
}

bool TimeSortMerger::OnStop(bool graceful) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Running) {
    return false;
  }

  fRunning = false;

  if (graceful) {
    // Wait for threads to finish processing
    for (auto &thread : fReceivingThreads) {
      if (thread && thread->joinable()) {
        thread->join();
      }
    }
    if (fMergingThread && fMergingThread->joinable()) {
      fMergingThread->join();
    }
    if (fSendingThread && fSendingThread->joinable()) {
      fSendingThread->join();
    }
  } else {
    // Detach threads for emergency stop
    for (auto &thread : fReceivingThreads) {
      if (thread) {
        thread->detach();
      }
    }
    if (fMergingThread) {
      fMergingThread->detach();
      fMergingThread.reset();
    }
    if (fSendingThread) {
      fSendingThread->detach();
      fSendingThread.reset();
    }
  }
  fReceivingThreads.clear();

  fState = ComponentState::Configured;
  return true;
}

void TimeSortMerger::OnReset() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  // Stop everything
  fRunning = false;
  fShutdownRequested = false;

  for (auto &thread : fReceivingThreads) {
    if (thread && thread->joinable()) {
      thread->join();
    }
  }
  fReceivingThreads.clear();

  if (fMergingThread && fMergingThread->joinable()) {
    fMergingThread->join();
  }
  if (fSendingThread && fSendingThread->joinable()) {
    fSendingThread->join();
  }

  // Reset state
  fErrorMessage.clear();
  fRunNumber = 0;
  fEventsProcessed = 0;
  fBytesTransferred = 0;

  // Disconnect transports
  for (auto &transport : fInputTransports) {
    if (transport) {
      transport->Disconnect();
    }
  }
  fInputTransports.clear();

  if (fOutputTransport) {
    fOutputTransport->Disconnect();
  }

  // Reset EOS tracker
  fEOSTracker->Reset();

  fState = ComponentState::Idle;
}

// === Helper methods ===

bool TimeSortMerger::TransitionTo(ComponentState newState) {
  ComponentState current = fState.load();
  if (IsValidTransition(current, newState)) {
    fState = newState;
    return true;
  }
  return false;
}

void TimeSortMerger::ReceivingLoop(size_t input_index) {
  if (input_index >= fInputTransports.size()) {
    return;
  }

  auto &transport = fInputTransports[input_index];

  while (fRunning) {
    // Check if transport is valid
    if (!transport || !transport->IsConnected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // Receive data from transport
    auto data = transport->ReceiveBytes();

    // Check fRunning again after potentially blocking receive
    if (!fRunning) {
      break;
    }

    if (data && !data->empty()) {
      size_t dataSize = data->size();

      // Decode events
      auto [events, sequence] = fDataProcessor->Decode(data);
      if (events && !events->empty()) {
        // TODO: Add events to merge buffer with input_index tag
        fEventsProcessed += events->size();
        fBytesTransferred += dataSize;
      }

      // TODO: Check for EOS marker and update tracker
      // if (isEOS) {
      //   fEOSTracker->ReceiveEOS("input_" + std::to_string(input_index));
      // }
    } else {
      // No data available, sleep briefly
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void TimeSortMerger::MergingLoop() {
  // TODO: Implement time-based sorting/merging
  // 1. Collect events from all input buffers
  // 2. Sort by timestamp
  // 3. Output events older than (newest_timestamp - sort_window)
  while (fRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void TimeSortMerger::SendingLoop() {
  // TODO: Implement sending of merged data
  while (fRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// === Command channel ===

void TimeSortMerger::SetCommandAddress(const std::string &address) {
  fCommandAddress = address;
}

std::string TimeSortMerger::GetCommandAddress() const { return fCommandAddress; }

void TimeSortMerger::StartCommandListener() {
  if (fCommandListenerRunning || fCommandAddress.empty()) {
    return;
  }

  // Create and configure command transport
  fCommandTransport = std::make_unique<Net::ZMQTransport>();
  Net::TransportConfig config;
  config.command_address = fCommandAddress;
  config.bind_command = true;
  // Disable data and status sockets
  config.data_address = "";
  config.status_address = "";

  if (!fCommandTransport->Configure(config) || !fCommandTransport->Connect()) {
    fCommandTransport.reset();
    return;
  }

  fCommandListenerRunning = true;
  fCommandListenerThread =
      std::make_unique<std::thread>(&TimeSortMerger::CommandListenerLoop, this);
}

void TimeSortMerger::StopCommandListener() {
  fCommandListenerRunning = false;

  if (fCommandListenerThread && fCommandListenerThread->joinable()) {
    fCommandListenerThread->join();
  }
  fCommandListenerThread.reset();

  if (fCommandTransport) {
    fCommandTransport->Disconnect();
    fCommandTransport.reset();
  }
}

void TimeSortMerger::CommandListenerLoop() {
  while (fCommandListenerRunning) {
    auto cmd = fCommandTransport->ReceiveCommand();
    if (cmd) {
      HandleCommand(*cmd);
    }
  }
}

void TimeSortMerger::HandleCommand(const Command &cmd) {
  bool success = false;
  std::string message;

  switch (cmd.type) {
  case CommandType::Configure:
    success = (fState == ComponentState::Idle);
    if (success) {
      success = Initialize("");
    } else if (fState == ComponentState::Configured) {
      success = true;
    }
    message = success ? "Configured" : "Failed to configure";
    break;

  case CommandType::Arm:
    success = Arm();
    message = success ? "Armed" : "Failed to arm";
    break;

  case CommandType::Start:
    success = Start(cmd.run_number);
    message = success ? "Started" : "Failed to start";
    break;

  case CommandType::Stop:
    success = Stop(cmd.graceful);
    message = success ? "Stopped" : "Failed to stop";
    break;

  case CommandType::Reset:
    Reset();
    success = true;
    message = "Reset";
    break;

  case CommandType::GetStatus:
    success = true;
    message = "Status OK";
    break;

  default:
    success = false;
    message = "Unknown command";
    break;
  }

  CommandResponse response;
  response.request_id = cmd.request_id;
  response.success = success;
  response.error_code = success ? ErrorCode::Success : ErrorCode::InvalidStateTransition;
  response.current_state = fState.load();
  response.message = message;

  fCommandTransport->SendCommandResponse(response);
}

}  // namespace DELILA
