#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

// DELILA components
#include <delila/monitor/Monitor.hpp>
#include <delila/monitor/Recorder.hpp>
#include <delila/net/ZMQTransport.hpp>
#include <delila/net/DataProcessor.hpp>
#include <delila/core/EventData.hpp>

using DELILA::Monitor::Monitor;
using DELILA::Monitor::Recorder;
using DELILA::Net::ZMQTransport;
using DELILA::Net::TransportConfig;
using DELILA::Net::DataProcessor;

enum class DAQState { Running, Stopping };

DAQState GetKBInput()
{
  // Set terminal to non-blocking mode
  struct termios old_tio, new_tio;
  tcgetattr(STDIN_FILENO, &old_tio);
  new_tio = old_tio;
  new_tio.c_lflag &= (~ICANON & ~ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

  // Set stdin to non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  char input;
  int result = read(STDIN_FILENO, &input, 1);

  // Restore terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
  fcntl(STDIN_FILENO, F_SETFL, flags);

  if (result > 0) {
    if (input == 'q' || input == 'Q') {
      return DAQState::Stopping;
    } else {
      return DAQState::Running;
    }
  } else {
    // No input available
    return DAQState::Running;
  }
}

int main(int argc, char* argv[])
{
  std::cout << "DELILA Data Sink" << std::endl;
  std::cout << "Press 'q' to quit" << std::endl;

  // Parse command line arguments
  std::string connect_address = "tcp://localhost:5555";
  bool enable_monitor = true;
  bool enable_recorder = true;
  bool enable_compression = false;
  bool enable_checksum = false;
  std::string output_prefix = "test";
  std::string output_dir = "./";

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--address" && i + 1 < argc) {
      connect_address = argv[++i];
    } else if (arg == "--no-monitor") {
      enable_monitor = false;
    } else if (arg == "--no-recorder") {
      enable_recorder = false;
    } else if (arg == "--compress") {
      enable_compression = true;
    } else if (arg == "--checksum") {
      enable_checksum = true;
    } else if (arg == "--output-prefix" && i + 1 < argc) {
      output_prefix = argv[++i];
    } else if (arg == "--output-dir" && i + 1 < argc) {
      output_dir = argv[++i];
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  --address <addr>       Connect address (default: tcp://localhost:5555)" << std::endl;
      std::cout << "  --no-monitor          Disable monitoring/visualization" << std::endl;
      std::cout << "  --no-recorder         Disable data recording" << std::endl;
      std::cout << "  --compress            Enable compression" << std::endl;
      std::cout << "  --checksum            Enable checksum verification" << std::endl;
      std::cout << "  --output-prefix <pre> Output file prefix (default: test)" << std::endl;
      std::cout << "  --output-dir <dir>    Output directory (default: ./)" << std::endl;
      return 0;
    }
  }

  std::cout << "Connect address: " << connect_address << std::endl;
  std::cout << "Monitor: " << (enable_monitor ? "enabled" : "disabled") << std::endl;
  std::cout << "Recorder: " << (enable_recorder ? "enabled" : "disabled") << std::endl;
  std::cout << "Compression: " << (enable_compression ? "enabled" : "disabled") << std::endl;
  std::cout << "Checksum: " << (enable_checksum ? "enabled" : "disabled") << std::endl;

  // Configure network transport
  ZMQTransport transport;
  TransportConfig net_config;
  net_config.data_address = connect_address;
  net_config.data_pattern = "SUB";
  net_config.bind_data = false;  // Connect as subscriber

  // Disable status and command channels for now
  net_config.status_address = net_config.data_address;
  net_config.command_address = net_config.data_address;

  if (!transport.Configure(net_config)) {
    std::cerr << "Failed to configure transport" << std::endl;
    return 1;
  }

  if (!transport.Connect()) {
    std::cerr << "Failed to connect transport" << std::endl;
    return 1;
  }

  // Configure data processor with optional features (must match source settings)
  DataProcessor processor;
  if (enable_compression) {
    processor.EnableCompression(true);
    std::cout << "Compression enabled" << std::endl;
  }
  if (enable_checksum) {
    processor.EnableChecksum(true);
    std::cout << "Checksum enabled" << std::endl;
  }

  std::cout << "Transport connected, waiting for data..." << std::endl;

  // Initialize monitor if enabled
  std::unique_ptr<Monitor> monitor;
  if (enable_monitor) {
    monitor = std::make_unique<Monitor>();
    monitor->EnableWebServer(true);
    monitor->SetNThreads(4);
    
    // Configure histograms
    auto histsParams = std::vector<std::vector<DELILA::Monitor::HistsParams>>{
        {{"ADC Histogram", "ADC Channel Histogram", 32000, 0.0, 32000.0}}};
    monitor->SetHistsParams(histsParams);
    
    // Create ADC histograms for 32 channels
    auto adcChannels = std::vector<uint32_t>{32};
    monitor->CreateADCHists(adcChannels);
    
    monitor->Start();
    std::cout << "Monitor started with web server" << std::endl;
  }

  // Initialize recorder if enabled
  std::unique_ptr<Recorder> recorder;
  if (enable_recorder) {
    recorder = std::make_unique<Recorder>();
    recorder->StartRecording(
        4,                    // Number of threads
        1024 * 1024 * 1024,   // 1GB file size limit
        100000,               // Events per file
        1,                    // Number of files (0 for unlimited)
        output_prefix,        // File prefix
        output_dir            // Output directory
    );
    std::cout << "Recorder started, output: " << output_dir << "/" << output_prefix << "_*.dat" << std::endl;
  }

  // Main processing loop
  bool isRunning = true;
  
  // Statistics
  auto startTime = std::chrono::steady_clock::now();
  uint64_t totalEvents = 0;
  uint64_t totalBatches = 0;
  uint64_t lastSequence = 0;
  uint64_t missedSequences = 0;
  
  auto lastStatsTime = startTime;
  uint64_t lastEventCount = 0;

  while (isRunning) {
    // Check for keyboard input
    auto state = GetKBInput();
    if (state == DAQState::Stopping) {
      isRunning = false;
      break;
    }

    // Receive event data from network using new API
    auto data = transport.ReceiveBytes();
    if (!data) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }

    auto [events, sequence] = processor.Decode(data);
    
    if (events && !events->empty()) {
      totalEvents += events->size();
      totalBatches++;

      // Check for missed sequences
      if (lastSequence > 0 && sequence != lastSequence + 1) {
        missedSequences += (sequence - lastSequence - 1);
        std::cerr << "Warning: Missed sequences " << lastSequence + 1 
                  << " to " << sequence - 1 << std::endl;
      }
      lastSequence = sequence;

      // Forward to monitor
      if (monitor) {
        // Create a copy for monitor (it takes ownership)
        auto monitorEvents = std::make_unique<std::vector<std::unique_ptr<DELILA::Digitizer::EventData>>>();
        for (const auto& event : *events) {
          auto eventCopy = std::make_unique<DELILA::Digitizer::EventData>(*event);
          monitorEvents->push_back(std::move(eventCopy));
        }
        monitor->LoadEventData(std::move(monitorEvents));
      }

      // Forward to recorder
      if (recorder) {
        // Recorder also needs ownership, so move the original
        recorder->LoadEventData(std::move(events));
      }
    }

    // Print statistics every second
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceStats = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - lastStatsTime).count();
    
    if (timeSinceStats >= 1) {
      uint64_t eventsInPeriod = totalEvents - lastEventCount;
      double rate = static_cast<double>(eventsInPeriod) / timeSinceStats;
      
      std::cout << "Events: " << totalEvents 
                << " | Batches: " << totalBatches
                << " | Rate: " << rate << " evt/s"
                << " | Seq: " << lastSequence
                << " | Missed: " << missedSequences << std::endl;
      
      lastStatsTime = currentTime;
      lastEventCount = totalEvents;
    }
  }

  std::cout << "\nStopping sink..." << std::endl;
  
  // Stop components
  if (monitor) {
    monitor->Stop();
  }
  if (recorder) {
    recorder->StopRecording();
  }
  transport.Disconnect();

  // Final statistics
  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  
  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events: " << totalEvents << std::endl;
  std::cout << "Total batches: " << totalBatches << std::endl;
  std::cout << "Missed sequences: " << missedSequences << std::endl;
  std::cout << "Elapsed time: " << elapsedTime.count() << " seconds" << std::endl;
  std::cout << "Average rate: " << totalEvents / elapsedTime.count() << " events/second" << std::endl;
  
  if (totalBatches > 0) {
    std::cout << "Average batch size: " << totalEvents / totalBatches << " events/batch" << std::endl;
  }

  return 0;
}