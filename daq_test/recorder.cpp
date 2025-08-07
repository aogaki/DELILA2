#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

// DELILA components
#include <delila/monitor/Recorder.hpp>
#include <delila/net/ZMQTransport.hpp>
#include <delila/core/EventData.hpp>

using DELILA::Monitor::Recorder;
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
  std::cout << "DELILA Data Recorder" << std::endl;
  std::cout << "Press 'q' to quit" << std::endl;

  // Parse command line arguments
  std::string connect_address = "tcp://localhost:5555";
  bool enable_compression = false;
  bool enable_checksum = false;
  std::string output_prefix = "data";
  std::string output_dir = "./";
  int num_threads = 4;
  uint64_t max_file_size = 1024 * 1024 * 1024;  // 1GB default
  uint64_t events_per_file = 100000;
  int max_files = 0;  // 0 for unlimited

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--address" && i + 1 < argc) {
      connect_address = argv[++i];
    } else if (arg == "--compress") {
      enable_compression = true;
    } else if (arg == "--checksum") {
      enable_checksum = true;
    } else if (arg == "--output-prefix" && i + 1 < argc) {
      output_prefix = argv[++i];
    } else if (arg == "--output-dir" && i + 1 < argc) {
      output_dir = argv[++i];
    } else if (arg == "--threads" && i + 1 < argc) {
      num_threads = std::stoi(argv[++i]);
    } else if (arg == "--max-file-size" && i + 1 < argc) {
      max_file_size = std::stoull(argv[++i]);
    } else if (arg == "--events-per-file" && i + 1 < argc) {
      events_per_file = std::stoull(argv[++i]);
    } else if (arg == "--max-files" && i + 1 < argc) {
      max_files = std::stoi(argv[++i]);
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  --address <addr>         Connect address (default: tcp://localhost:5555)" << std::endl;
      std::cout << "  --compress               Enable compression" << std::endl;
      std::cout << "  --checksum               Enable checksum verification" << std::endl;
      std::cout << "  --output-prefix <prefix> Output file prefix (default: data)" << std::endl;
      std::cout << "  --output-dir <dir>       Output directory (default: ./)" << std::endl;
      std::cout << "  --threads <n>            Number of threads (default: 4)" << std::endl;
      std::cout << "  --max-file-size <bytes>  Max file size in bytes (default: 1GB)" << std::endl;
      std::cout << "  --events-per-file <n>    Events per file (default: 100000)" << std::endl;
      std::cout << "  --max-files <n>          Max number of files, 0=unlimited (default: 0)" << std::endl;
      return 0;
    }
  }

  std::cout << "Connect address: " << connect_address << std::endl;
  std::cout << "Compression: " << (enable_compression ? "enabled" : "disabled") << std::endl;
  std::cout << "Checksum: " << (enable_checksum ? "enabled" : "disabled") << std::endl;
  std::cout << "Output: " << output_dir << "/" << output_prefix << "_*.dat" << std::endl;
  std::cout << "Threads: " << num_threads << std::endl;
  std::cout << "Max file size: " << max_file_size / (1024*1024) << " MB" << std::endl;
  std::cout << "Events per file: " << events_per_file << std::endl;
  std::cout << "Max files: " << (max_files == 0 ? "unlimited" : std::to_string(max_files)) << std::endl;

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

  // Initialize recorder
  auto recorder = std::make_unique<Recorder>();
  recorder->StartRecording(
      num_threads,
      max_file_size,
      events_per_file,
      max_files,
      output_prefix,
      output_dir
  );
  
  std::cout << "Recorder started" << std::endl;

  // Main recording loop
  bool isRunning = true;
  
  // Statistics
  auto startTime = std::chrono::steady_clock::now();
  uint64_t totalEvents = 0;
  uint64_t totalBatches = 0;
  uint64_t lastSequence = 0;
  uint64_t missedSequences = 0;
  uint64_t filesCreated = 0;
  
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

      // Forward to recorder (it takes ownership)
      recorder->LoadEventData(std::move(events));
      
      // Check if new file was created (simplified check)
      uint64_t currentFileNumber = totalEvents / events_per_file;
      if (currentFileNumber > filesCreated) {
        filesCreated = currentFileNumber;
        std::cout << "New file created: " << output_prefix << "_" 
                  << filesCreated << ".dat" << std::endl;
      }
    }

    // Print statistics every second
    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceStats = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - lastStatsTime).count();
    
    if (timeSinceStats >= 1) {
      uint64_t eventsInPeriod = totalEvents - lastEventCount;
      double rate = static_cast<double>(eventsInPeriod) / timeSinceStats;
      
      std::cout << "[RECORDER] Events: " << totalEvents 
                << " | Batches: " << totalBatches
                << " | Rate: " << rate << " evt/s"
                << " | Files: " << filesCreated
                << " | Missed: " << missedSequences << std::endl;
      
      lastStatsTime = currentTime;
      lastEventCount = totalEvents;
    }
  }

  std::cout << "\nStopping recorder..." << std::endl;
  
  // Stop components
  recorder->StopRecording();
  transport.Disconnect();

  // Final statistics
  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  
  std::cout << "\n=== Recorder Final Statistics ===" << std::endl;
  std::cout << "Total events: " << totalEvents << std::endl;
  std::cout << "Total batches: " << totalBatches << std::endl;
  std::cout << "Files created: " << filesCreated << std::endl;
  std::cout << "Missed sequences: " << missedSequences << std::endl;
  std::cout << "Elapsed time: " << elapsedTime.count() << " seconds" << std::endl;
  std::cout << "Average rate: " << totalEvents / elapsedTime.count() << " events/second" << std::endl;
  
  if (totalBatches > 0) {
    std::cout << "Average batch size: " << totalEvents / totalBatches << " events/batch" << std::endl;
  }

  // List created files
  std::cout << "\nData files created in " << output_dir << ":" << std::endl;
  for (uint64_t i = 1; i <= filesCreated; i++) {
    std::cout << "  " << output_prefix << "_" << i << ".dat" << std::endl;
  }

  return 0;
}