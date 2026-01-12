/**
 * @file Emulator.cpp
 * @brief Digitizer emulator component implementation
 */

#include "Emulator.hpp"

#include <DataProcessor.hpp>
#include <ZMQTransport.hpp>
#include <delila/core/CommandResponse.hpp>
#include <delila/core/ErrorCode.hpp>
#include <delila/core/EventData.hpp>
#include <delila/core/MinimalEventData.hpp>

#include <chrono>

namespace DELILA {

Emulator::Emulator()
    : fTransport(std::make_unique<Net::ZMQTransport>()),
      fDataProcessor(std::make_unique<Net::DataProcessor>()) {
  // Seed with random device by default
  std::random_device rd;
  fRng.seed(rd());
}

Emulator::~Emulator() { Shutdown(); }

// === IComponent interface ===

bool Emulator::Initialize(const std::string& config_path) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Idle) {
    return false;
  }

  // Output address is required
  if (fOutputAddresses.empty()) {
    fErrorMessage = "No output address configured";
    return false;
  }

  // Apply custom seed if set
  if (fSeedSet) {
    fRng.seed(fSeed);
  }

  // Configure transport
  Net::TransportConfig transportConfig;
  transportConfig.data_address = fOutputAddresses[0];
  transportConfig.bind_data = true;
  transportConfig.data_pattern = "PUSH";
  // Disable status and command sockets
  transportConfig.status_address = transportConfig.data_address;
  transportConfig.command_address = "";

  if (!fTransport->Configure(transportConfig)) {
    fErrorMessage = "Failed to configure transport";
    fState = ComponentState::Error;
    return false;
  }

  fState = ComponentState::Configured;
  return true;
}

void Emulator::Run() {
  // Main loop - wait for shutdown
  while (!fShutdownRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void Emulator::Shutdown() {
  fShutdownRequested = true;
  fRunning = false;

  // Stop command listener first
  StopCommandListener();

  // Stop generation thread
  if (fGenerationThread && fGenerationThread->joinable()) {
    fGenerationThread->join();
  }
  fGenerationThread.reset();

  // Disconnect transport
  if (fTransport) {
    fTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

ComponentState Emulator::GetState() const { return fState.load(); }

std::string Emulator::GetComponentId() const { return fComponentId; }

ComponentStatus Emulator::GetStatus() const {
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

void Emulator::SetInputAddresses(
    const std::vector<std::string>& /*addresses*/) {
  // Emulator has no inputs - ignore
}

void Emulator::SetOutputAddresses(const std::vector<std::string>& addresses) {
  fOutputAddresses = addresses;
}

std::vector<std::string> Emulator::GetInputAddresses() const {
  return {};  // Always empty for emulator
}

std::vector<std::string> Emulator::GetOutputAddresses() const {
  return fOutputAddresses;
}

// === Command channel ===

void Emulator::SetCommandAddress(const std::string& address) {
  fCommandAddress = address;
}

std::string Emulator::GetCommandAddress() const { return fCommandAddress; }

void Emulator::StartCommandListener() {
  if (fCommandListenerRunning || fCommandAddress.empty()) {
    return;
  }

  // Create and configure command transport
  fCommandTransport = std::make_unique<Net::ZMQTransport>();
  Net::TransportConfig config;
  config.command_address = fCommandAddress;
  config.bind_command = true;
  config.data_address = "";
  config.status_address = "";

  if (!fCommandTransport->Configure(config) || !fCommandTransport->Connect()) {
    fCommandTransport.reset();
    return;
  }

  fCommandListenerRunning = true;
  fCommandListenerThread =
      std::make_unique<std::thread>(&Emulator::CommandListenerLoop, this);
}

void Emulator::StopCommandListener() {
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

// === Public control methods ===

bool Emulator::Arm() { return OnArm(); }

bool Emulator::Start(uint32_t run_number) { return OnStart(run_number); }

bool Emulator::Stop(bool graceful) { return OnStop(graceful); }

void Emulator::Reset() { OnReset(); }

// === Configuration ===

void Emulator::SetComponentId(const std::string& id) { fComponentId = id; }

void Emulator::SetModuleNumber(uint8_t module) { fModuleNumber = module; }

uint8_t Emulator::GetModuleNumber() const { return fModuleNumber; }

void Emulator::SetNumChannels(uint8_t num) { fNumChannels = num; }

uint8_t Emulator::GetNumChannels() const { return fNumChannels; }

void Emulator::SetEventRate(uint32_t rate) { fEventRate = rate; }

uint32_t Emulator::GetEventRate() const { return fEventRate; }

void Emulator::SetDataMode(EmulatorDataMode mode) { fDataMode = mode; }

EmulatorDataMode Emulator::GetDataMode() const { return fDataMode; }

void Emulator::SetEnergyRange(uint16_t min, uint16_t max) {
  fEnergyMin = min;
  fEnergyMax = max;
}

std::pair<uint16_t, uint16_t> Emulator::GetEnergyRange() const {
  return {fEnergyMin, fEnergyMax};
}

void Emulator::SetWaveformSize(size_t size) { fWaveformSize = size; }

size_t Emulator::GetWaveformSize() const { return fWaveformSize; }

void Emulator::SetSeed(uint64_t seed) {
  fSeed = seed;
  fSeedSet = true;
}

// === Testing utilities ===

void Emulator::ForceError(const std::string& message) {
  fErrorMessage = message;
  fState = ComponentState::Error;
}

// === IComponent callbacks ===

bool Emulator::OnConfigure(const nlohmann::json& /*config*/) {
  // Already handled in Initialize
  return true;
}

bool Emulator::OnArm() {
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

bool Emulator::OnStart(uint32_t run_number) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Armed) {
    return false;
  }

  fRunNumber = run_number;
  fEventsProcessed = 0;
  fBytesTransferred = 0;
  fCurrentTimestampNs = 0.0;
  fRunning = true;

  // Reset sequence number in data processor
  fDataProcessor->ResetSequence();

  // Start generation thread
  fGenerationThread =
      std::make_unique<std::thread>(&Emulator::GenerationLoop, this);

  fState = ComponentState::Running;
  return true;
}

bool Emulator::OnStop(bool graceful) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Running) {
    return false;
  }

  fRunning = false;

  if (graceful) {
    // Wait for generation thread to finish
    if (fGenerationThread && fGenerationThread->joinable()) {
      fGenerationThread->join();
    }

    // Send EOS marker
    if (fDataProcessor && fTransport && fTransport->IsConnected()) {
      auto eosMessage = fDataProcessor->CreateEOSMessage();
      if (eosMessage) {
        fTransport->SendBytes(eosMessage);
      }
    }
  } else {
    // Emergency stop - detach thread
    if (fGenerationThread) {
      fGenerationThread->detach();
    }
  }
  fGenerationThread.reset();

  fState = ComponentState::Configured;
  return true;
}

void Emulator::OnReset() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  // Stop everything
  fRunning = false;
  fShutdownRequested = false;

  if (fGenerationThread && fGenerationThread->joinable()) {
    fGenerationThread->join();
  }
  fGenerationThread.reset();

  // Reset state
  fErrorMessage.clear();
  fRunNumber = 0;
  fEventsProcessed = 0;
  fBytesTransferred = 0;
  fCurrentTimestampNs = 0.0;

  // Disconnect transport
  if (fTransport) {
    fTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

// === Helper methods ===

bool Emulator::TransitionTo(ComponentState newState) {
  ComponentState current = fState.load();
  if (IsValidTransition(current, newState)) {
    fState = newState;
    return true;
  }
  return false;
}

void Emulator::GenerationLoop() {
  // Calculate interval between events in nanoseconds
  const double intervalNs = 1e9 / static_cast<double>(fEventRate);

  // Distribution for channel and energy
  std::uniform_int_distribution<uint8_t> channelDist(0, fNumChannels - 1);
  std::uniform_int_distribution<uint16_t> energyDist(fEnergyMin, fEnergyMax);

  // Small jitter for timestamp (±10%)
  std::uniform_real_distribution<double> jitterDist(0.9, 1.1);

  while (fRunning) {
    // Generate timestamp with jitter
    fCurrentTimestampNs += intervalNs * jitterDist(fRng);

    if (fDataMode == EmulatorDataMode::Minimal) {
      // Generate MinimalEventData
      auto events =
          std::make_unique<std::vector<std::unique_ptr<Digitizer::MinimalEventData>>>();

      auto event = std::make_unique<Digitizer::MinimalEventData>();
      event->module = fModuleNumber;
      event->channel = channelDist(fRng);
      event->timeStampNs = fCurrentTimestampNs;
      event->energy = energyDist(fRng);
      event->energyShort = static_cast<uint16_t>(event->energy * 0.8);
      event->flags = 0;

      events->push_back(std::move(event));

      // Serialize and send
      auto data = fDataProcessor->ProcessWithAutoSequence(events);
      if (data && fTransport && fTransport->IsConnected()) {
        size_t dataSize = data->size();
        if (fTransport->SendBytes(data)) {
          fEventsProcessed++;
          fBytesTransferred += dataSize;
        }
      }
    } else {
      // Generate full EventData
      auto events =
          std::make_unique<std::vector<std::unique_ptr<Digitizer::EventData>>>();

      auto event = std::make_unique<Digitizer::EventData>(fWaveformSize);
      event->module = fModuleNumber;
      event->channel = channelDist(fRng);
      event->timeStampNs = fCurrentTimestampNs;
      event->energy = energyDist(fRng);
      event->energyShort = static_cast<uint16_t>(event->energy * 0.8);
      event->flags = 0;

      // Generate waveform if enabled
      if (fWaveformSize > 0) {
        std::uniform_int_distribution<int32_t> waveformDist(0, 4095);
        std::uniform_int_distribution<uint8_t> digitalDist(0, 1);

        for (size_t i = 0; i < fWaveformSize; ++i) {
          event->analogProbe1[i] = waveformDist(fRng);
          event->analogProbe2[i] = waveformDist(fRng);
          event->digitalProbe1[i] = digitalDist(fRng);
          event->digitalProbe2[i] = digitalDist(fRng);
          event->digitalProbe3[i] = digitalDist(fRng);
          event->digitalProbe4[i] = digitalDist(fRng);
        }
      }

      events->push_back(std::move(event));

      // Serialize and send
      auto data = fDataProcessor->ProcessWithAutoSequence(events);
      if (data && fTransport && fTransport->IsConnected()) {
        size_t dataSize = data->size();
        if (fTransport->SendBytes(data)) {
          fEventsProcessed++;
          fBytesTransferred += dataSize;
        }
      }
    }

    // Sleep to maintain event rate
    auto sleepTime = std::chrono::microseconds(
        static_cast<int64_t>(intervalNs / 1000.0));
    std::this_thread::sleep_for(sleepTime);
  }
}

void Emulator::CommandListenerLoop() {
  while (fCommandListenerRunning) {
    auto cmd = fCommandTransport->ReceiveCommand();
    if (cmd) {
      HandleCommand(*cmd);
    }
  }
}

void Emulator::HandleCommand(const Command& cmd) {
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
  response.error_code =
      success ? ErrorCode::Success : ErrorCode::InvalidStateTransition;
  response.current_state = fState.load();
  response.message = message;

  fCommandTransport->SendCommandResponse(response);
}

}  // namespace DELILA
