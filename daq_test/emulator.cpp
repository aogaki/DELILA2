#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <random>

// DELILA components
#include <delila/net/ZMQTransport.hpp>
#include <delila/core/EventData.hpp>

using DELILA::Net::ZMQTransport;
using DELILA::Net::TransportConfig;
using DELILA::Digitizer::EventData;

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

// Generate synthetic event data
std::unique_ptr<EventData> GenerateEvent(uint32_t eventId, uint32_t channelId, 
                                         size_t dataSize, std::mt19937& gen, 
                                         std::uniform_int_distribution<uint16_t>& dist)
{
  auto event = std::make_unique<EventData>();
  
  // Set basic event properties using correct field names
  event->channel = static_cast<uint8_t>(channelId);
  event->module = 0;
  event->timeStampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch()).count();
  event->energy = dist(gen);
  event->energyShort = event->energy / 2;
  
  // Generate analog probe data of specified size
  size_t samples = dataSize / sizeof(int32_t);
  event->analogProbe1.resize(samples);
  for (auto& sample : event->analogProbe1) {
    sample = static_cast<int32_t>(dist(gen));
  }
  
  // Set some realistic probe configurations
  event->analogProbe1Type = 0;  // Input signal
  event->analogProbe2Type = 1;  // RC-CR signal
  event->timeResolution = 2;    // 2ns
  event->flags = 0;             // No flags set
  
  return event;
}

int main(int argc, char* argv[])
{
  std::cout << "DELILA Data Emulator" << std::endl;
  std::cout << "Press 'q' to quit" << std::endl;

  // Parse command line arguments
  std::string bind_address = "tcp://*:5555";
  bool enable_compression = false;
  bool enable_checksum = false;
  size_t event_data_size = 8192;  // 8KB per event by default
  uint32_t events_per_batch = 100;
  uint32_t num_channels = 32;
  uint32_t pool_size = 1000;
  bool unlimited_speed = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--address" && i + 1 < argc) {
      bind_address = argv[++i];
    } else if (arg == "--compress") {
      enable_compression = true;
    } else if (arg == "--checksum") {
      enable_checksum = true;
    } else if (arg == "--event-size" && i + 1 < argc) {
      event_data_size = std::stoull(argv[++i]);
    } else if (arg == "--batch-size" && i + 1 < argc) {
      events_per_batch = std::stoul(argv[++i]);
    } else if (arg == "--channels" && i + 1 < argc) {
      num_channels = std::stoul(argv[++i]);
    } else if (arg == "--pool-size" && i + 1 < argc) {
      pool_size = std::stoul(argv[++i]);
    } else if (arg == "--unlimited") {
      unlimited_speed = true;
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  --address <addr>     Bind address (default: tcp://*:5555)" << std::endl;
      std::cout << "  --compress           Enable compression" << std::endl;
      std::cout << "  --checksum           Enable checksum" << std::endl;
      std::cout << "  --event-size <bytes> Event data size in bytes (default: 8192)" << std::endl;
      std::cout << "  --batch-size <n>     Events per batch (default: 100)" << std::endl;
      std::cout << "  --channels <n>       Number of channels (default: 32)" << std::endl;
      std::cout << "  --pool-size <n>      Memory pool size (default: 1000)" << std::endl;
      std::cout << "  --unlimited          Send at unlimited speed (no sleep)" << std::endl;
      return 0;
    }
  }

  std::cout << "Configuration:" << std::endl;
  std::cout << "  Bind address: " << bind_address << std::endl;
  std::cout << "  Event size: " << event_data_size << " bytes" << std::endl;
  std::cout << "  Batch size: " << events_per_batch << " events" << std::endl;
  std::cout << "  Channels: " << num_channels << std::endl;
  std::cout << "  Pool size: " << pool_size << std::endl;
  std::cout << "  Compression: " << (enable_compression ? "enabled" : "disabled") << std::endl;
  std::cout << "  Checksum: " << (enable_checksum ? "enabled" : "disabled") << std::endl;
  std::cout << "  Speed: " << (unlimited_speed ? "unlimited" : "limited") << std::endl;
  std::cout << "  Total data per batch: " << (event_data_size * events_per_batch) / 1024 << " KB" << std::endl;

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
  transport.SetMemoryPoolSize(pool_size);

  if (!transport.Connect()) {
    std::cerr << "Failed to connect transport" << std::endl;
    return 1;
  }

  std::cout << "Transport connected, starting data emission..." << std::endl;

  // Pre-generate template events for each channel to avoid runtime allocation
  std::cout << "Pre-generating template events..." << std::endl;
  
  std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<uint16_t> dist(100, 4000);
  
  std::vector<std::vector<std::unique_ptr<EventData>>> templateEvents(num_channels);
  for (uint32_t ch = 0; ch < num_channels; ch++) {
    templateEvents[ch].reserve(events_per_batch);
    for (uint32_t i = 0; i < events_per_batch; i++) {
      templateEvents[ch].push_back(GenerateEvent(i, ch, event_data_size, gen, dist));
    }
  }
  
  std::cout << "Template events ready. Starting emission..." << std::endl;

  // Main emission loop
  bool isEmitting = true;
  
  // Statistics
  auto startTime = std::chrono::steady_clock::now();
  uint64_t totalEvents = 0;
  uint64_t totalBatches = 0;
  uint32_t currentChannel = 0;
  uint64_t sequenceNumber = 1;
  
  auto lastStatsTime = startTime;
  uint64_t lastEventCount = 0;

  while (isEmitting) {
    // Check for keyboard input
    auto state = GetKBInput();
    if (state == DAQState::Stopping) {
      break;
    }

    // Create a batch by copying template events from current channel
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    events->reserve(events_per_batch);
    
    for (uint32_t i = 0; i < events_per_batch; i++) {
      // Copy template event (expensive but realistic for testing)
      auto event = std::make_unique<EventData>(*templateEvents[currentChannel][i]);
      
      // Update with unique sequence info
      event->timeStampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      
      events->push_back(std::move(event));
    }
    
    // Send the batch
    if (transport.Send(events)) {
      totalEvents += events_per_batch;
      totalBatches++;
      sequenceNumber++;
      
      // Rotate through channels
      currentChannel = (currentChannel + 1) % num_channels;
    } else {
      std::cerr << "Failed to send batch" << std::endl;
    }

    // Add small delay if not unlimited speed
    if (!unlimited_speed) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Print statistics every second
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceStats = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - lastStatsTime).count();
    
    if (timeSinceStats >= 1) {
      uint64_t eventsInPeriod = totalEvents - lastEventCount;
      double rate = static_cast<double>(eventsInPeriod) / timeSinceStats;
      double dataRateMBps = (rate * event_data_size) / (1024.0 * 1024.0);
      
      std::cout << "Events: " << totalEvents 
                << " | Batches: " << totalBatches
                << " | Rate: " << rate << " evt/s"
                << " | Data: " << dataRateMBps << " MB/s"
                << " | Pool: " << transport.GetPooledContainerCount() 
                << "/" << transport.GetMemoryPoolSize() 
                << " | Ch: " << currentChannel << std::endl;
      
      lastStatsTime = currentTime;
      lastEventCount = totalEvents;
    }
  }

  std::cout << "\nStopping emulator..." << std::endl;
  
  // Cleanup
  transport.Disconnect();

  // Final statistics
  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  double totalDataMB = (totalEvents * event_data_size) / (1024.0 * 1024.0);
  
  std::cout << "\n=== Emulator Final Statistics ===" << std::endl;
  std::cout << "Total events: " << totalEvents << std::endl;
  std::cout << "Total batches: " << totalBatches << std::endl;
  std::cout << "Total data: " << totalDataMB << " MB" << std::endl;
  std::cout << "Elapsed time: " << elapsedTime.count() << " seconds" << std::endl;
  std::cout << "Average event rate: " << totalEvents / elapsedTime.count() << " events/second" << std::endl;
  std::cout << "Average data rate: " << totalDataMB / elapsedTime.count() << " MB/second" << std::endl;
  
  if (totalBatches > 0) {
    std::cout << "Average batch size: " << totalEvents / totalBatches << " events/batch" << std::endl;
  }

  return 0;
}