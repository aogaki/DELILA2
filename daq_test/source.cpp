#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

// DELILA components
#include <delila/digitizer/Digitizer.hpp>
#include <delila/digitizer/ConfigurationManager.hpp>
#include <delila/net/ZMQTransport.hpp>

using DELILA::Digitizer::ConfigurationManager;
using DELILA::Digitizer::Digitizer;
using DELILA::Net::ZMQTransport;
using DELILA::Net::TransportConfig;

enum class DAQState { Acquiring, Stopping };

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
      return DAQState::Acquiring;
    }
  } else {
    // No input available
    return DAQState::Acquiring;
  }
}

int main(int argc, char* argv[])
{
  std::cout << "DELILA Data Source" << std::endl;
  std::cout << "Press 'q' to quit" << std::endl;

  // Parse command line arguments
  std::string config_file = "PSD2.conf";
  std::string bind_address = "tcp://*:5555";
  bool enable_compression = false;
  bool enable_checksum = false;

  if (argc > 1) {
    config_file = argv[1];
  }
  if (argc > 2) {
    bind_address = argv[2];
  }
  if (argc > 3 && std::string(argv[3]) == "--compress") {
    enable_compression = true;
  }
  if (argc > 4 && std::string(argv[4]) == "--checksum") {
    enable_checksum = true;
  }

  std::cout << "Configuration file: " << config_file << std::endl;
  std::cout << "Bind address: " << bind_address << std::endl;
  std::cout << "Compression: " << (enable_compression ? "enabled" : "disabled") << std::endl;
  std::cout << "Checksum: " << (enable_checksum ? "enabled" : "disabled") << std::endl;

  // Initialize digitizer
  auto digitizer = std::make_unique<Digitizer>();
  auto configManager = ConfigurationManager();
  
  try {
    configManager.LoadFromFile(config_file);
    digitizer->Initialize(configManager);
    digitizer->Configure();
  } catch (const std::exception& e) {
    std::cerr << "Failed to initialize digitizer: " << e.what() << std::endl;
    return 1;
  }

  // Configure network transport
  ZMQTransport transport;
  TransportConfig net_config;
  net_config.data_address = bind_address;
  net_config.data_pattern = "PUB";
  net_config.bind_data = true;
  
  // Disable status and command channels for now
  net_config.status_address = net_config.data_address;
  net_config.command_address = net_config.data_address;

  if (!transport.Configure(net_config)) {
    std::cerr << "Failed to configure transport" << std::endl;
    return 1;
  }

  // Enable optional features
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
  transport.SetMemoryPoolSize(1000); // Pool of 1000 event containers

  if (!transport.Connect()) {
    std::cerr << "Failed to connect transport" << std::endl;
    return 1;
  }

  std::cout << "Transport connected, starting acquisition..." << std::endl;

  // Start data acquisition
  digitizer->StartAcquisition();
  bool isAcquiring = true;

  // Statistics
  auto startTime = std::chrono::steady_clock::now();
  uint64_t totalEvents = 0;
  uint64_t totalBatches = 0;
  uint64_t emptyBatches = 0;
  
  auto lastStatsTime = startTime;
  uint64_t lastEventCount = 0;

  while (isAcquiring) {
    // Check for keyboard input
    auto state = GetKBInput();
    if (state == DAQState::Stopping) {
      break;
    }

    // Get event data from digitizer
    auto events = digitizer->GetEventData();
    
    if (events && !events->empty()) {
      totalEvents += events->size();
      totalBatches++;

      // Send via network
      if (!transport.Send(events)) {
        std::cerr << "Failed to send events" << std::endl;
      }
    } else {
      emptyBatches++;
      // Small sleep to avoid busy waiting when no events
      std::this_thread::sleep_for(std::chrono::microseconds(100));
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
                << " | Pool usage: " << transport.GetPooledContainerCount() 
                << "/" << transport.GetMemoryPoolSize() << std::endl;
      
      lastStatsTime = currentTime;
      lastEventCount = totalEvents;
    }
  }

  std::cout << "\nStopping acquisition..." << std::endl;
  
  // Stop and cleanup
  digitizer->StopAcquisition();
  transport.Disconnect();

  // Final statistics
  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  
  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events: " << totalEvents << std::endl;
  std::cout << "Total batches: " << totalBatches << std::endl;
  std::cout << "Empty batches: " << emptyBatches << std::endl;
  std::cout << "Elapsed time: " << elapsedTime.count() << " seconds" << std::endl;
  std::cout << "Average rate: " << totalEvents / elapsedTime.count() << " events/second" << std::endl;
  
  if (totalBatches > 0) {
    std::cout << "Average batch size: " << totalEvents / totalBatches << " events/batch" << std::endl;
  }

  return 0;
}