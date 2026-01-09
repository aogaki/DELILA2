#include "FileWriter.hpp"
#include <DataProcessor.hpp>
#include <ZMQTransport.hpp>
#include <delila/core/CommandResponse.hpp>
#include <delila/core/ErrorCode.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace DELILA {

FileWriter::FileWriter()
    : fTransport(std::make_unique<Net::ZMQTransport>()),
      fDataProcessor(std::make_unique<Net::DataProcessor>()) {}

FileWriter::~FileWriter() { Shutdown(); }

// === IComponent interface ===

bool FileWriter::Initialize(const std::string &config_path) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Idle) {
    return false;
  }

  // Load configuration from file if provided
  if (!config_path.empty()) {
    // TODO: Load configuration from file
  }

  // Configure transport if we have input addresses
  if (!fInputAddresses.empty()) {
    Net::TransportConfig transportConfig;
    transportConfig.data_address = fInputAddresses[0];
    transportConfig.bind_data = false; // Connect to upstream
    transportConfig.data_pattern = "PULL";
    // Disable status and command sockets (not needed for FileWriter)
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

void FileWriter::Run() {
  // Main loop - wait for shutdown
  while (!fShutdownRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void FileWriter::Shutdown() {
  fShutdownRequested = true;
  fRunning = false;

  // Stop command listener first
  StopCommandListener();

  // Stop worker threads
  if (fReceivingThread && fReceivingThread->joinable()) {
    fReceivingThread->join();
  }
  if (fWritingThread && fWritingThread->joinable()) {
    fWritingThread->join();
  }

  // Close file and disconnect transport
  CloseOutputFile();
  if (fTransport) {
    fTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

ComponentState FileWriter::GetState() const { return fState.load(); }

std::string FileWriter::GetComponentId() const { return fComponentId; }

ComponentStatus FileWriter::GetStatus() const {
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

void FileWriter::SetInputAddresses(const std::vector<std::string> &addresses) {
  fInputAddresses = addresses;
}

void FileWriter::SetOutputAddresses(
    const std::vector<std::string> & /*addresses*/) {
  // FileWriter has no outputs - ignore
}

std::vector<std::string> FileWriter::GetInputAddresses() const {
  return fInputAddresses;
}

std::vector<std::string> FileWriter::GetOutputAddresses() const {
  return {}; // Always empty for sink
}

// === Public control methods ===

bool FileWriter::Arm() { return OnArm(); }

bool FileWriter::Start(uint32_t run_number) { return OnStart(run_number); }

bool FileWriter::Stop(bool graceful) { return OnStop(graceful); }

void FileWriter::Reset() { OnReset(); }

// === Configuration ===

void FileWriter::SetComponentId(const std::string &id) { fComponentId = id; }

void FileWriter::SetOutputPath(const std::string &path) { fOutputPath = path; }

std::string FileWriter::GetOutputPath() const { return fOutputPath; }

void FileWriter::SetFilePrefix(const std::string &prefix) {
  fFilePrefix = prefix;
}

std::string FileWriter::GetFilePrefix() const { return fFilePrefix; }

// === Testing utilities ===

void FileWriter::ForceError(const std::string &message) {
  fErrorMessage = message;
  fState = ComponentState::Error;
}

// === IComponent callbacks ===

bool FileWriter::OnConfigure(const nlohmann::json & /*config*/) {
  // Already handled in Initialize
  return true;
}

bool FileWriter::OnArm() {
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

bool FileWriter::OnStart(uint32_t run_number) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Armed) {
    return false;
  }

  fRunNumber = run_number;
  fEventsProcessed = 0;
  fBytesTransferred = 0;
  fReceivedEOS = false;  // Reset EOS flag for new run

  // Open output file
  if (!OpenOutputFile(run_number)) {
    fErrorMessage = "Failed to open output file";
    fState = ComponentState::Error;
    return false;
  }

  fRunning = true;

  // Start worker threads
  fReceivingThread =
      std::make_unique<std::thread>(&FileWriter::ReceivingLoop, this);

  fState = ComponentState::Running;
  return true;
}

bool FileWriter::OnStop(bool graceful) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Running) {
    return false;
  }

  fRunning = false;

  if (graceful) {
    // Wait for threads to finish processing
    if (fReceivingThread && fReceivingThread->joinable()) {
      fReceivingThread->join();
    }
    if (fWritingThread && fWritingThread->joinable()) {
      fWritingThread->join();
    }
  } else {
    // Detach threads for emergency stop
    if (fReceivingThread) {
      fReceivingThread->detach();
      fReceivingThread.reset();
    }
    if (fWritingThread) {
      fWritingThread->detach();
      fWritingThread.reset();
    }
  }

  // Close output file
  CloseOutputFile();

  fState = ComponentState::Configured;
  return true;
}

void FileWriter::OnReset() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  // Stop everything
  fRunning = false;
  fShutdownRequested = false;

  if (fReceivingThread && fReceivingThread->joinable()) {
    fReceivingThread->join();
  }
  if (fWritingThread && fWritingThread->joinable()) {
    fWritingThread->join();
  }

  // Reset state
  fErrorMessage.clear();
  fRunNumber = 0;
  fEventsProcessed = 0;
  fBytesTransferred = 0;

  // Close file and disconnect transport
  CloseOutputFile();
  if (fTransport) {
    fTransport->Disconnect();
  }

  fState = ComponentState::Idle;
}

// === Helper methods ===

bool FileWriter::TransitionTo(ComponentState newState) {
  ComponentState current = fState.load();
  if (IsValidTransition(current, newState)) {
    fState = newState;
    return true;
  }
  return false;
}

void FileWriter::ReceivingLoop() {
  while (fRunning) {
    // Check if transport is valid before receiving
    if (!fTransport || !fTransport->IsConnected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // Receive data from transport
    auto data = fTransport->ReceiveBytes();

    // Check fRunning again after potentially blocking receive
    if (!fRunning) {
      break;
    }

    if (data && !data->empty()) {
      // Check for EOS (End Of Stream) marker
      if (Net::DataProcessor::IsEOSMessage(data->data(), data->size())) {
        fReceivedEOS.store(true);
        // EOS received - upstream has finished sending data
        // Continue running to allow graceful shutdown
        continue;
      }

      // Store the size before any operations
      size_t dataSize = data->size();
      const uint8_t *dataPtr = data->data();

      // Decode events
      auto [events, sequence] = fDataProcessor->Decode(data);
      if (events && !events->empty()) {
        // Write to file - use stored values since data is still valid
        if (fOutputFile && fOutputFile->is_open()) {
          fOutputFile->write(reinterpret_cast<const char *>(dataPtr),
                             static_cast<std::streamsize>(dataSize));
          fEventsProcessed += events->size();
          fBytesTransferred += dataSize;
        }
      }
    } else {
      // No data available, sleep briefly
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void FileWriter::WritingLoop() {
  // TODO: Implement queue-based writing for decoupling
  while (fRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

std::string FileWriter::GenerateFilename(uint32_t run_number) const {
  std::ostringstream oss;
  oss << fFilePrefix << std::setfill('0') << std::setw(6) << run_number
      << ".dat";
  return oss.str();
}

bool FileWriter::OpenOutputFile(uint32_t run_number) {
  std::string filename = GenerateFilename(run_number);
  std::string full_path = fOutputPath;
  if (!full_path.empty() && full_path.back() != '/') {
    full_path += '/';
  }
  full_path += filename;

  fOutputFile = std::make_unique<std::ofstream>(full_path, std::ios::binary);
  return fOutputFile && fOutputFile->is_open();
}

void FileWriter::CloseOutputFile() {
  if (fOutputFile) {
    if (fOutputFile->is_open()) {
      fOutputFile->close();
    }
    fOutputFile.reset();
  }
}

// === Command channel ===

void FileWriter::SetCommandAddress(const std::string &address) {
  fCommandAddress = address;
}

std::string FileWriter::GetCommandAddress() const { return fCommandAddress; }

void FileWriter::StartCommandListener() {
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
      std::make_unique<std::thread>(&FileWriter::CommandListenerLoop, this);
}

void FileWriter::StopCommandListener() {
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

void FileWriter::CommandListenerLoop() {
  while (fCommandListenerRunning) {
    auto cmd = fCommandTransport->ReceiveCommand();
    if (cmd) {
      HandleCommand(*cmd);
    }
  }
}

void FileWriter::HandleCommand(const Command &cmd) {
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

// === EOS (End Of Stream) tracking ===

bool FileWriter::HasReceivedEOS() const { return fReceivedEOS.load(); }

void FileWriter::ResetEOSFlag() { fReceivedEOS.store(false); }

} // namespace DELILA
