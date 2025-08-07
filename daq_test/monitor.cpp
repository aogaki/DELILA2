#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

// DELILA components
#include <delila/monitor/Monitor.hpp>
#include <delila/net/ZMQTransport.hpp>
#include <delila/core/EventData.hpp>

using DELILA::Monitor::Monitor;
using DELILA::Net::ZMQTransport;
using DELILA::Net::TransportConfig;

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
  std::cout << "DELILA Data Monitor" << std::endl;
  std::cout << "Press 'q' to quit" << std::endl;

  // Parse command line arguments
  std::string connect_address = "tcp://localhost:5555";
  bool enable_web = true;
  bool enable_compression = false;
  bool enable_checksum = false;
  int num_threads = 4;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--address" && i + 1 < argc) {
      connect_address = argv[++i];
    } else if (arg == "--no-web") {
      enable_web = false;
    } else if (arg == "--compress") {
      enable_compression = true;
    } else if (arg == "--checksum") {
      enable_checksum = true;
    } else if (arg == "--threads" && i + 1 < argc) {
      num_threads = std::stoi(argv[++i]);
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  --address <addr>  Connect address (default: tcp://localhost:5555)" << std::endl;
      std::cout << "  --no-web          Disable web server" << std::endl;
      std::cout << "  --compress        Enable compression" << std::endl;
      std::cout << "  --checksum        Enable checksum verification" << std::endl;
      std::cout << "  --threads <n>     Number of threads (default: 4)" << std::endl;
      return 0;
    }
  }

  std::cout << "Connect address: " << connect_address << std::endl;
  std::cout << "Web server: " << (enable_web ? "enabled" : "disabled") << std::endl;
  std::cout << "Compression: " << (enable_compression ? "enabled" : "disabled") << std::endl;
  std::cout << "Checksum: " << (enable_checksum ? "enabled" : "disabled") << std::endl;
  std::cout << "Threads: " << num_threads << std::endl;

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

  // Enable optional features (must match source settings)
  if (enable_compression) {
    transport.EnableCompression(true);
    std::cout << "Compression enabled" << std::endl;
  }
  if (enable_checksum) {
    transport.EnableChecksum(true);
    std::cout << "Checksum enabled" << std::endl;
  }

  // Enable memory pool for better performance
  transport.EnableMemoryPool(true);
  transport.SetMemoryPoolSize(1000);

  if (!transport.Connect()) {
    std::cerr << "Failed to connect transport" << std::endl;
    return 1;
  }

  std::cout << "Transport connected, waiting for data..." << std::endl;

  // Initialize monitor
  auto monitor = std::make_unique<Monitor>();
  monitor->EnableWebServer(enable_web);
  monitor->SetNThreads(num_threads);
  
  // Configure histograms
  auto histsParams = std::vector<std::vector<DELILA::Monitor::HistsParams>>{
      {{"ADC Histogram", "ADC Channel Histogram", 32000, 0.0, 32000.0}}};
  monitor->SetHistsParams(histsParams);
  
  // Create ADC histograms for 32 channels
  auto adcChannels = std::vector<uint32_t>{32};
  monitor->CreateADCHists(adcChannels);
  
  monitor->Start();
  
  if (enable_web) {
    std::cout << "Monitor started with web server at http://localhost:8080" << std::endl;
  } else {
    std::cout << "Monitor started (web server disabled)" << std::endl;
  }

  // Main monitoring loop
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

    // Receive event data from network
    auto [events, sequence] = transport.Receive();
    
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

      // Forward to monitor (it takes ownership)
      monitor->LoadEventData(std::move(events));
    }

    // Print statistics every second
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceStats = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - lastStatsTime).count();
    
    if (timeSinceStats >= 1) {
      uint64_t eventsInPeriod = totalEvents - lastEventCount;
      double rate = static_cast<double>(eventsInPeriod) / timeSinceStats;
      
      std::cout << "[MONITOR] Events: " << totalEvents 
                << " | Batches: " << totalBatches
                << " | Rate: " << rate << " evt/s"
                << " | Seq: " << lastSequence
                << " | Missed: " << missedSequences << std::endl;
      
      lastStatsTime = currentTime;
      lastEventCount = totalEvents;
    }
  }

  std::cout << "\nStopping monitor..." << std::endl;
  
  // Stop components
  monitor->Stop();
  transport.Disconnect();

  // Final statistics
  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  
  std::cout << "\n=== Monitor Final Statistics ===" << std::endl;
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