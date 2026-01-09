#include "DigitizerSource.hpp"
#include <DataProcessor.hpp>
#include <ZMQTransport.hpp>
#include <delila/core/CommandResponse.hpp>
#include <delila/core/ErrorCode.hpp>

#include <chrono>
#include <iostream>

namespace DELILA {

DigitizerSource::DigitizerSource()
    : fTransport(std::make_unique<Net::ZMQTransport>()),
      fDataProcessor(std::make_unique<Net::DataProcessor>()) {}

DigitizerSource::~DigitizerSource() { Shutdown(); }

// === IComponent interface ===

bool DigitizerSource::Initialize(const std::string &config_path) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Idle) {
    return false;
  }

  // In mock mode, we don't need actual configuration
  if (!fMockMode && !config_path.empty()) {
    // TODO: Load configuration from file
  }

  // Configure transport if we have output addresses
  if (!fOutputAddresses.empty()) {
    Net::TransportConfig transportConfig;
    transportConfig.data_address = fOutputAddresses[0];
    transportConfig.bind_data = true;
    transportConfig.data_pattern = "PUSH";
    // Disable status and command sockets (not needed for DigitizerSource)
    transportConfig.status_address = transportConfig.data_address;
    transportConfig.command_address = "";

    if (!fTransport->Configure(transportConfig)) {
      fErrorMessage = "Failed to configure transport";
      fState = ComponentState::Error;
      return false;
    }
  }

  fState = ComponentState::Configured;
  return true;
}

void DigitizerSource::Run() {
  // Main loop - wait for shutdown
  while (!fShutdownRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void DigitizerSource::Shutdown() {
  fShutdownRequested = true;
  fRunning = false;

  // Stop command listener first
  StopCommandListener();

  // Stop worker threads
  if (fAcquisitionThread && fAcquisitionThread->joinable()) {
    fAcquisitionThread->join();
  }
  if (fSendingThread && fSendingThread->joinable()) {
    fSendingThread->join();
  }

  // Disconnect transport
  if (fTransport) {
    fTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

ComponentState DigitizerSource::GetState() const { return fState.load(); }

std::string DigitizerSource::GetComponentId() const { return fComponentId; }

ComponentStatus DigitizerSource::GetStatus() const {
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

void DigitizerSource::SetInputAddresses(
    const std::vector<std::string> & /*addresses*/) {
  // DigitizerSource has no inputs - ignore
}

void DigitizerSource::SetOutputAddresses(
    const std::vector<std::string> &addresses) {
  fOutputAddresses = addresses;
}

std::vector<std::string> DigitizerSource::GetInputAddresses() const {
  return {}; // Always empty for source
}

std::vector<std::string> DigitizerSource::GetOutputAddresses() const {
  return fOutputAddresses;
}

// === Public control methods ===

bool DigitizerSource::Arm() { return OnArm(); }

bool DigitizerSource::Start(uint32_t run_number) { return OnStart(run_number); }

bool DigitizerSource::Stop(bool graceful) { return OnStop(graceful); }

void DigitizerSource::Reset() { OnReset(); }

// === Configuration ===

void DigitizerSource::SetComponentId(const std::string &id) {
  fComponentId = id;
}

void DigitizerSource::SetMockMode(bool enable) { fMockMode = enable; }

void DigitizerSource::SetMockEventRate(uint32_t events_per_second) {
  fMockEventRate = events_per_second;
}

// === Testing utilities ===

void DigitizerSource::ForceError(const std::string &message) {
  fErrorMessage = message;
  fState = ComponentState::Error;
}

// === IComponent callbacks ===

bool DigitizerSource::OnConfigure(const nlohmann::json & /*config*/) {
  // Already handled in Initialize
  return true;
}

bool DigitizerSource::OnArm() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Configured) {
    return false;
  }

  // Connect transport
  if (fTransport && !fTransport->IsConnected()) {
    if (!fTransport->Connect()) {
      fErrorMessage = "Failed to connect transport";
      fState = ComponentState::Error;
      return false;
    }
  }

  fState = ComponentState::Armed;
  return true;
}

bool DigitizerSource::OnStart(uint32_t run_number) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Armed) {
    return false;
  }

  fRunNumber = run_number;
  fEventsProcessed = 0;
  fBytesTransferred = 0;
  fRunning = true;

  // Start worker threads
  fAcquisitionThread =
      std::make_unique<std::thread>(&DigitizerSource::AcquisitionLoop, this);

  fState = ComponentState::Running;
  return true;
}

bool DigitizerSource::OnStop(bool graceful) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Running) {
    return false;
  }

  fRunning = false;

  if (graceful) {
    // Wait for threads to finish processing
    if (fAcquisitionThread && fAcquisitionThread->joinable()) {
      fAcquisitionThread->join();
    }
    if (fSendingThread && fSendingThread->joinable()) {
      fSendingThread->join();
    }

    // Send EOS (End Of Stream) marker after all data has been sent
    if (fDataProcessor && fTransport && fTransport->IsConnected()) {
      auto eosMessage = fDataProcessor->CreateEOSMessage();
      if (eosMessage) {
        fTransport->SendBytes(eosMessage);
      }
    }
  } else {
    // Detach threads for emergency stop - no EOS sent
    if (fAcquisitionThread) {
      fAcquisitionThread->detach();
      fAcquisitionThread.reset();
    }
    if (fSendingThread) {
      fSendingThread->detach();
      fSendingThread.reset();
    }
  }

  fState = ComponentState::Configured;
  return true;
}

void DigitizerSource::OnReset() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  // Stop everything
  fRunning = false;
  fShutdownRequested = false;

  if (fAcquisitionThread && fAcquisitionThread->joinable()) {
    fAcquisitionThread->join();
  }
  if (fSendingThread && fSendingThread->joinable()) {
    fSendingThread->join();
  }

  // Reset state
  fErrorMessage.clear();
  fRunNumber = 0;
  fEventsProcessed = 0;
  fBytesTransferred = 0;

  // Disconnect transport
  if (fTransport) {
    fTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

// === Helper methods ===

bool DigitizerSource::TransitionTo(ComponentState newState) {
  ComponentState current = fState.load();
  if (IsValidTransition(current, newState)) {
    fState = newState;
    return true;
  }
  return false;
}

void DigitizerSource::AcquisitionLoop() {
  while (fRunning) {
    if (fMockMode) {
      GenerateMockEvents();
    } else {
      // TODO: Read from actual digitizer
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

void DigitizerSource::SendingLoop() {
  // TODO: Implement queue-based sending
  while (fRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void DigitizerSource::GenerateMockEvents() {
  // Calculate sleep time based on event rate
  auto sleepTime = std::chrono::microseconds(1000000 / fMockEventRate);

  // Create mock event
  auto events =
      std::make_unique<std::vector<std::unique_ptr<Digitizer::EventData>>>();

  auto event = std::make_unique<Digitizer::EventData>();
  event->module = 0;
  event->channel = 0;
  event->timeStampNs = static_cast<double>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count());
  event->energy = static_cast<uint16_t>(1000 + (std::rand() % 1000));
  event->energyShort = 0;
  event->flags = 0;
  events->push_back(std::move(event));

  // Serialize and send
  auto data = fDataProcessor->ProcessWithAutoSequence(events);
  if (data && fTransport && fTransport->IsConnected()) {
    // Store size before SendBytes (which resets the unique_ptr)
    size_t dataSize = data->size();
    if (fTransport->SendBytes(data)) {
      fEventsProcessed++;
      fBytesTransferred += dataSize;
    }
  }

  std::this_thread::sleep_for(sleepTime);
}

// === Command channel ===

void DigitizerSource::SetCommandAddress(const std::string &address) {
  fCommandAddress = address;
}

std::string DigitizerSource::GetCommandAddress() const {
  return fCommandAddress;
}

void DigitizerSource::StartCommandListener() {
  if (fCommandListenerRunning || fCommandAddress.empty()) {
    return;
  }

  // Create and configure command transport
  fCommandTransport = std::make_unique<Net::ZMQTransport>();
  Net::TransportConfig config;
  config.command_address = fCommandAddress;
  config.bind_command = true;
  // Disable data and status sockets by using same address (they will be skipped)
  config.data_address = "";
  config.status_address = "";

  if (!fCommandTransport->Configure(config) || !fCommandTransport->Connect()) {
    fCommandTransport.reset();
    return;
  }

  fCommandListenerRunning = true;
  fCommandListenerThread =
      std::make_unique<std::thread>(&DigitizerSource::CommandListenerLoop, this);
}

void DigitizerSource::StopCommandListener() {
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

void DigitizerSource::CommandListenerLoop() {
  while (fCommandListenerRunning) {
    auto cmd = fCommandTransport->ReceiveCommand();
    if (cmd) {
      HandleCommand(*cmd);
    }
  }
}

void DigitizerSource::HandleCommand(const Command &cmd) {
  bool success = false;
  std::string message;

  switch (cmd.type) {
  case CommandType::Configure:
    // Configure is handled during Initialize, just accept it
    success = (fState == ComponentState::Idle);
    if (success) {
      // Trigger Initialize if in Idle state
      success = Initialize("");
    } else if (fState == ComponentState::Configured) {
      // Already configured, just return success
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

} // namespace DELILA
