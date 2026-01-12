/**
 * @file merger_main.cpp
 * @brief SimpleMerger executable
 *
 * Receives data from multiple upstream sources and forwards to downstream.
 * Does NOT perform time-sorting, just merges streams.
 *
 * Usage:
 *   delila_merger [options]
 *
 * Options:
 *   -i, --input <address>    ZMQ input address (can be specified multiple times)
 *   -o, --output <address>   ZMQ output address (default: tcp://*:5560)
 *   -h, --help               Show this help message
 *
 * Example:
 *   # Merge from two emulators and output on port 5560
 *   delila_merger -i tcp://localhost:5555 -i tcp://localhost:5556 -o tcp://*:5560
 */

#include <SimpleMerger.hpp>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace DELILA;

// Global pointer for signal handler
static SimpleMerger* g_merger = nullptr;
static volatile bool g_running = true;

void signalHandler(int signum) {
  std::cout << "\nReceived signal " << signum << ", shutting down..."
            << std::endl;
  g_running = false;
  if (g_merger) {
    g_merger->Stop(true);
  }
}

void printUsage(const char* program) {
  std::cout << "DELILA2 SimpleMerger - Multi-Source Data Merger\n\n";
  std::cout << "Usage: " << program << " [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -i, --input <address>    ZMQ input address (multiple allowed)\n";
  std::cout << "  -o, --output <address>   ZMQ output address (default: tcp://*:5560)\n";
  std::cout << "  -h, --help               Show this help message\n\n";
  std::cout << "Example:\n";
  std::cout << "  " << program << " -i tcp://localhost:5555 -i tcp://localhost:5556 -o tcp://*:5560\n";
}

int main(int argc, char* argv[]) {
  // Default configuration
  std::vector<std::string> input_addresses;
  std::string output_address = "tcp://*:5560";

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "-i" || arg == "--input") {
      if (i + 1 < argc) {
        input_addresses.push_back(argv[++i]);
      }
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 < argc) {
        output_address = argv[++i];
      }
    }
  }

  // Validate inputs
  if (input_addresses.empty()) {
    std::cerr << "ERROR: At least one input address is required (-i option)\n";
    printUsage(argv[0]);
    return 1;
  }

  // Print configuration
  std::cout << "=== DELILA2 SimpleMerger ===" << std::endl;
  std::cout << "Input addresses:" << std::endl;
  for (const auto& addr : input_addresses) {
    std::cout << "  - " << addr << std::endl;
  }
  std::cout << "Output address: " << output_address << std::endl;
  std::cout << std::endl;

  // Setup signal handlers
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  // Create and configure merger
  SimpleMerger merger;
  g_merger = &merger;

  merger.SetComponentId("merger");
  merger.SetInputAddresses(input_addresses);
  merger.SetOutputAddresses({output_address});

  // Initialize
  std::cout << "Initializing merger..." << std::endl;
  if (!merger.Initialize("")) {
    std::cerr << "ERROR: Failed to initialize merger" << std::endl;
    return 1;
  }

  // Arm
  std::cout << "Arming merger..." << std::endl;
  if (!merger.Arm()) {
    std::cerr << "ERROR: Failed to arm merger" << std::endl;
    return 1;
  }

  // Start with run number 1
  std::cout << "Starting merger (Run 1)..." << std::endl;
  if (!merger.Start(1)) {
    std::cerr << "ERROR: Failed to start merger" << std::endl;
    return 1;
  }

  std::cout << "Merger running. Press Ctrl+C to stop." << std::endl;

  // Main loop - print status periodically
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (g_running) {
      auto status = merger.GetStatus();
      std::cout << "[Status] Events: " << status.metrics.events_processed
                << ", Bytes: " << status.metrics.bytes_transferred << std::endl;
    }
  }

  // Cleanup
  std::cout << "Stopping merger..." << std::endl;
  merger.Stop(true);
  merger.Shutdown();

  auto status = merger.GetStatus();
  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events:     " << status.metrics.events_processed << std::endl;
  std::cout << "Total bytes:      " << status.metrics.bytes_transferred << std::endl;

  g_merger = nullptr;
  return 0;
}
