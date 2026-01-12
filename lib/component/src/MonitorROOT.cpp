/**
 * @file MonitorROOT.cpp
 * @brief ROOT-based online monitor component implementation
 */

#include "MonitorROOT.hpp"

#include <DataProcessor.hpp>
#include <ZMQTransport.hpp>
#include <delila/core/CommandResponse.hpp>
#include <delila/core/ErrorCode.hpp>
#include <delila/core/EventData.hpp>
#include <delila/core/MinimalEventData.hpp>

#include <TGraph.h>
#include <TH1F.h>
#include <TH1I.h>
#include <TH2F.h>
#include <THttpServer.h>
#include <TROOT.h>

#include <chrono>

namespace DELILA {

MonitorROOT::MonitorROOT()
    : fTransport(std::make_unique<Net::ZMQTransport>()),
      fDataProcessor(std::make_unique<Net::DataProcessor>()) {
  // Enable ROOT thread safety
  ROOT::EnableThreadSafety();
}

MonitorROOT::~MonitorROOT() { Shutdown(); }

// === IComponent interface ===

bool MonitorROOT::Initialize(const std::string& config_path) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Idle) {
    return false;
  }

  // Input address is required
  if (fInputAddresses.empty()) {
    fErrorMessage = "No input address configured";
    return false;
  }

  // Configure transport
  Net::TransportConfig transportConfig;
  transportConfig.data_address = fInputAddresses[0];
  transportConfig.bind_data = false;  // Connect to upstream
  transportConfig.data_pattern = "PULL";
  // Disable status and command sockets
  transportConfig.status_address = transportConfig.data_address;
  transportConfig.command_address = "";

  if (!fTransport->Configure(transportConfig)) {
    fErrorMessage = "Failed to configure transport";
    fState = ComponentState::Error;
    return false;
  }

  // Create HTTP server
  std::string serverArg = "http:" + std::to_string(fHttpPort);
  fHttpServer = std::make_unique<THttpServer>(serverArg.c_str());

  if (!fHttpServer) {
    fErrorMessage = "Failed to create THttpServer";
    fState = ComponentState::Error;
    return false;
  }

  // Create histograms
  CreateHistograms();

  fState = ComponentState::Configured;
  return true;
}

void MonitorROOT::Run() {
  // Main loop - wait for shutdown
  while (!fShutdownRequested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void MonitorROOT::Shutdown() {
  fShutdownRequested = true;
  fRunning = false;

  // Stop command listener first
  StopCommandListener();

  // Stop receive thread
  if (fReceiveThread && fReceiveThread->joinable()) {
    fReceiveThread->join();
  }
  fReceiveThread.reset();

  // Disconnect transport
  if (fTransport) {
    fTransport->Disconnect();
  }

  // Delete histograms before server
  DeleteHistograms();

  // Stop HTTP server
  fHttpServer.reset();

  fState = ComponentState::Idle;
}

ComponentState MonitorROOT::GetState() const { return fState.load(); }

std::string MonitorROOT::GetComponentId() const { return fComponentId; }

ComponentStatus MonitorROOT::GetStatus() const {
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

void MonitorROOT::SetInputAddresses(const std::vector<std::string>& addresses) {
  fInputAddresses = addresses;
}

void MonitorROOT::SetOutputAddresses(
    const std::vector<std::string>& /*addresses*/) {
  // MonitorROOT has no outputs - ignore
}

std::vector<std::string> MonitorROOT::GetInputAddresses() const {
  return fInputAddresses;
}

std::vector<std::string> MonitorROOT::GetOutputAddresses() const {
  return {};  // Always empty for monitor
}

// === Command channel ===

void MonitorROOT::SetCommandAddress(const std::string& address) {
  fCommandAddress = address;
}

std::string MonitorROOT::GetCommandAddress() const { return fCommandAddress; }

void MonitorROOT::StartCommandListener() {
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
      std::make_unique<std::thread>(&MonitorROOT::CommandListenerLoop, this);
}

void MonitorROOT::StopCommandListener() {
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

bool MonitorROOT::Arm() { return OnArm(); }

bool MonitorROOT::Start(uint32_t run_number) { return OnStart(run_number); }

bool MonitorROOT::Stop(bool graceful) { return OnStop(graceful); }

void MonitorROOT::Reset() { OnReset(); }

// === Configuration ===

void MonitorROOT::SetComponentId(const std::string& id) { fComponentId = id; }

void MonitorROOT::SetHttpPort(int port) { fHttpPort = port; }

int MonitorROOT::GetHttpPort() const { return fHttpPort; }

void MonitorROOT::SetUpdateInterval(uint32_t ms) { fUpdateInterval = ms; }

uint32_t MonitorROOT::GetUpdateInterval() const { return fUpdateInterval; }

// === Histogram configuration ===

void MonitorROOT::EnableEnergyHistogram(bool enable) {
  fEnableEnergyHist = enable;
}

bool MonitorROOT::IsEnergyHistogramEnabled() const { return fEnableEnergyHist; }

void MonitorROOT::EnableChannelHistogram(bool enable) {
  fEnableChannelHist = enable;
}

bool MonitorROOT::IsChannelHistogramEnabled() const {
  return fEnableChannelHist;
}

void MonitorROOT::EnableTimingHistogram(bool enable) {
  fEnableTimingHist = enable;
}

bool MonitorROOT::IsTimingHistogramEnabled() const { return fEnableTimingHist; }

void MonitorROOT::Enable2DHistogram(bool enable) { fEnable2DHist = enable; }

bool MonitorROOT::Is2DHistogramEnabled() const { return fEnable2DHist; }

// === Waveform display configuration ===

void MonitorROOT::EnableWaveformDisplay(bool enable) { fEnableWaveform = enable; }

bool MonitorROOT::IsWaveformDisplayEnabled() const { return fEnableWaveform; }

void MonitorROOT::SetWaveformChannel(uint8_t module, uint8_t channel) {
  fWaveformModule = module;
  fWaveformChannel = channel;
}

std::pair<uint8_t, uint8_t> MonitorROOT::GetWaveformChannel() const {
  return {fWaveformModule, fWaveformChannel};
}

// === Testing utilities ===

void MonitorROOT::ForceError(const std::string& message) {
  fErrorMessage = message;
  fState = ComponentState::Error;
}

// === IComponent callbacks ===

bool MonitorROOT::OnConfigure(const nlohmann::json& /*config*/) {
  // Already handled in Initialize
  return true;
}

bool MonitorROOT::OnArm() {
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

bool MonitorROOT::OnStart(uint32_t run_number) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Armed) {
    return false;
  }

  fRunNumber = run_number;
  fEventsProcessed = 0;
  fBytesTransferred = 0;
  fPreviousTimestamp = 0.0;
  fRunning = true;

  // Reset histograms for new run
  ResetHistograms();

  // Start receive thread
  fReceiveThread =
      std::make_unique<std::thread>(&MonitorROOT::ReceiveLoop, this);

  fState = ComponentState::Running;
  return true;
}

bool MonitorROOT::OnStop(bool graceful) {
  std::lock_guard<std::mutex> lock(fStateMutex);

  if (fState != ComponentState::Running) {
    return false;
  }

  fRunning = false;

  if (graceful) {
    // Wait for receive thread to finish
    if (fReceiveThread && fReceiveThread->joinable()) {
      fReceiveThread->join();
    }
  } else {
    // Emergency stop - detach thread
    if (fReceiveThread) {
      fReceiveThread->detach();
    }
  }
  fReceiveThread.reset();

  fState = ComponentState::Configured;
  return true;
}

void MonitorROOT::OnReset() {
  std::lock_guard<std::mutex> lock(fStateMutex);

  // Stop everything
  fRunning = false;
  fShutdownRequested = false;

  if (fReceiveThread && fReceiveThread->joinable()) {
    fReceiveThread->join();
  }
  fReceiveThread.reset();

  // Reset state
  fErrorMessage.clear();
  fRunNumber = 0;
  fEventsProcessed = 0;
  fBytesTransferred = 0;
  fPreviousTimestamp = 0.0;

  // Disconnect transport
  if (fTransport) {
    fTransport->Disconnect();
  }

  // Reset histograms
  ResetHistograms();

  fState = ComponentState::Idle;
}

// === Helper methods ===

bool MonitorROOT::TransitionTo(ComponentState newState) {
  ComponentState current = fState.load();
  if (IsValidTransition(current, newState)) {
    fState = newState;
    return true;
  }
  return false;
}

void MonitorROOT::ReceiveLoop() {
  while (fRunning) {
    // Receive raw bytes from transport
    auto data = fTransport->ReceiveBytes();

    // Check fRunning again after potentially blocking receive
    if (!fRunning) {
      break;
    }

    if (!data || data->empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    // Check for EOS message
    if (fDataProcessor->IsEOSMessage(*data)) {
      break;
    }

    // Try to decode as MinimalEventData first
    auto [minimalEvents, minimalSeq] = fDataProcessor->DecodeMinimal(data);

    if (minimalEvents && !minimalEvents->empty()) {
      std::lock_guard<std::mutex> lock(fHistMutex);

      for (const auto& event : *minimalEvents) {
        // Fill energy histogram
        if (fHEnergy && fEnableEnergyHist) {
          fHEnergy->Fill(event->energy);
        }

        // Fill channel histogram
        if (fHChannel && fEnableChannelHist) {
          fHChannel->Fill(event->channel);
        }

        // Fill module histogram
        if (fHModule) {
          fHModule->Fill(event->module);
        }

        // Fill timing histogram
        if (fHTimeDiff && fEnableTimingHist) {
          if (fPreviousTimestamp > 0) {
            double timeDiff =
                (event->timeStampNs - fPreviousTimestamp) / 1000.0;  // us
            fHTimeDiff->Fill(timeDiff);
          }
          fPreviousTimestamp = event->timeStampNs;
        }

        // Fill 2D histogram
        if (fHEnergyVsChannel && fEnable2DHist) {
          fHEnergyVsChannel->Fill(event->channel, event->energy);
        }
        if (fHEnergyVsModule && fEnable2DHist) {
          fHEnergyVsModule->Fill(event->module, event->energy);
        }

        fEventsProcessed++;
      }
      fBytesTransferred += data->size();
      continue;
    }

    // Try to decode as full EventData
    auto [fullEvents, fullSeq] = fDataProcessor->Decode(data);

    if (fullEvents && !fullEvents->empty()) {
      std::lock_guard<std::mutex> lock(fHistMutex);

      for (const auto& event : *fullEvents) {
        // Fill energy histogram
        if (fHEnergy && fEnableEnergyHist) {
          fHEnergy->Fill(event->energy);
        }

        // Fill channel histogram
        if (fHChannel && fEnableChannelHist) {
          fHChannel->Fill(event->channel);
        }

        // Fill module histogram
        if (fHModule) {
          fHModule->Fill(event->module);
        }

        // Fill timing histogram
        if (fHTimeDiff && fEnableTimingHist) {
          if (fPreviousTimestamp > 0) {
            double timeDiff =
                (event->timeStampNs - fPreviousTimestamp) / 1000.0;  // us
            fHTimeDiff->Fill(timeDiff);
          }
          fPreviousTimestamp = event->timeStampNs;
        }

        // Fill 2D histogram
        if (fHEnergyVsChannel && fEnable2DHist) {
          fHEnergyVsChannel->Fill(event->channel, event->energy);
        }
        if (fHEnergyVsModule && fEnable2DHist) {
          fHEnergyVsModule->Fill(event->module, event->energy);
        }

        // Update waveform if enabled and matching channel
        if (fGWaveform && fEnableWaveform && event->module == fWaveformModule &&
            event->channel == fWaveformChannel && event->waveformSize > 0) {
          fGWaveform->Set(event->waveformSize);
          for (size_t i = 0; i < event->waveformSize; ++i) {
            fGWaveform->SetPoint(i, static_cast<double>(i),
                                 static_cast<double>(event->analogProbe1[i]));
          }
        }

        fEventsProcessed++;
      }
      fBytesTransferred += data->size();
    }
  }
}

void MonitorROOT::CommandListenerLoop() {
  while (fCommandListenerRunning) {
    auto cmd = fCommandTransport->ReceiveCommand();
    if (cmd) {
      HandleCommand(*cmd);
    }
  }
}

void MonitorROOT::HandleCommand(const Command& cmd) {
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

// === Histogram management ===

void MonitorROOT::CreateHistograms() {
  std::lock_guard<std::mutex> lock(fHistMutex);

  // Energy histogram
  if (fEnableEnergyHist) {
    fHEnergy = new TH1F("hEnergy", "Energy Distribution;Energy;Counts", 16384,
                        0, 16384);
    if (fHttpServer) {
      fHttpServer->Register("/Histograms/Energy", fHEnergy);
    }
  }

  // Channel histogram
  if (fEnableChannelHist) {
    fHChannel =
        new TH1I("hChannel", "Channel Distribution;Channel;Counts", 64, 0, 64);
    if (fHttpServer) {
      fHttpServer->Register("/Histograms/Channel", fHChannel);
    }
  }

  // Module histogram (always created)
  fHModule =
      new TH1I("hModule", "Module Distribution;Module;Counts", 16, 0, 16);
  if (fHttpServer) {
    fHttpServer->Register("/Histograms/Module", fHModule);
  }

  // Timing histogram
  if (fEnableTimingHist) {
    fHTimeDiff = new TH1F("hTimeDiff", "Time Difference;Time Diff [us];Counts",
                          1000, 0, 1000);
    if (fHttpServer) {
      fHttpServer->Register("/Histograms/TimeDiff", fHTimeDiff);
    }
  }

  // 2D histograms
  if (fEnable2DHist) {
    fHEnergyVsChannel =
        new TH2F("hEnergyVsChannel", "Energy vs Channel;Channel;Energy", 64, 0,
                 64, 1024, 0, 16384);
    fHEnergyVsModule = new TH2F("hEnergyVsModule", "Energy vs Module;Module;Energy",
                                16, 0, 16, 1024, 0, 16384);
    if (fHttpServer) {
      fHttpServer->Register("/Histograms/2D/EnergyVsChannel", fHEnergyVsChannel);
      fHttpServer->Register("/Histograms/2D/EnergyVsModule", fHEnergyVsModule);
    }
  }

  // Waveform graph
  if (fEnableWaveform) {
    fGWaveform = new TGraph();
    fGWaveform->SetName("gWaveform");
    fGWaveform->SetTitle("Waveform;Sample;ADC");
    if (fHttpServer) {
      fHttpServer->Register("/Waveform", fGWaveform);
    }
  }
}

void MonitorROOT::ResetHistograms() {
  std::lock_guard<std::mutex> lock(fHistMutex);

  if (fHEnergy) fHEnergy->Reset();
  if (fHChannel) fHChannel->Reset();
  if (fHModule) fHModule->Reset();
  if (fHTimeDiff) fHTimeDiff->Reset();
  if (fHEnergyVsChannel) fHEnergyVsChannel->Reset();
  if (fHEnergyVsModule) fHEnergyVsModule->Reset();
  if (fGWaveform) fGWaveform->Set(0);
}

void MonitorROOT::DeleteHistograms() {
  std::lock_guard<std::mutex> lock(fHistMutex);

  // Unregister from server first (if server exists)
  // Note: ROOT manages histogram memory, so we just delete

  delete fHEnergy;
  fHEnergy = nullptr;
  delete fHChannel;
  fHChannel = nullptr;
  delete fHModule;
  fHModule = nullptr;
  delete fHTimeDiff;
  fHTimeDiff = nullptr;
  delete fHEnergyVsChannel;
  fHEnergyVsChannel = nullptr;
  delete fHEnergyVsModule;
  fHEnergyVsModule = nullptr;
  delete fGWaveform;
  fGWaveform = nullptr;
}

}  // namespace DELILA
