/**
 * @file writer_main.cpp
 * @brief FileWriter executable
 *
 * Receives data from upstream and writes to binary files.
 *
 * Usage:
 *   delila_writer [options]
 *
 * Options:
 *   -i, --input <address>    ZMQ input address (default: tcp://localhost:5560)
 *   -d, --dir <path>         Output directory (default: current directory)
 *   -p, --prefix <string>    File prefix (default: run_)
 *   -h, --help               Show this help message
 *
 * Output files:
 *   Files are named: <prefix><run_number>.dat
 *   Example: run_00001.dat
 *
 * Example:
 *   # Write data from merger to files in ./data directory
 *   delila_writer -i tcp://localhost:5560 -d ./data -p experiment_
 */

#include <FileWriter.hpp>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using namespace DELILA;

// Global pointer for signal handler
static FileWriter* g_writer = nullptr;
static volatile bool g_running = true;

void signalHandler(int signum) {
  std::cout << "\nReceived signal " << signum << ", shutting down..."
            << std::endl;
  g_running = false;
  if (g_writer) {
    g_writer->Stop(true);
  }
}

void printUsage(const char* program) {
  std::cout << "DELILA2 FileWriter - Binary Data Writer\n\n";
  std::cout << "Usage: " << program << " [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -i, --input <address>    ZMQ input address (default: tcp://localhost:5560)\n";
  std::cout << "  -d, --dir <path>         Output directory (default: current directory)\n";
  std::cout << "  -p, --prefix <string>    File prefix (default: run_)\n";
  std::cout << "  -h, --help               Show this help message\n\n";
  std::cout << "Output files:\n";
  std::cout << "  Files are named: <prefix><run_number>.dat\n";
  std::cout << "  Example: run_00001.dat\n\n";
  std::cout << "Example:\n";
  std::cout << "  " << program << " -i tcp://localhost:5560 -d ./data -p experiment_\n";
}

int main(int argc, char* argv[]) {
  // Default configuration
  std::string input_address = "tcp://localhost:5560";
  std::string output_dir = ".";
  std::string file_prefix = "run_";

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "-i" || arg == "--input") {
      if (i + 1 < argc) {
        input_address = argv[++i];
      }
    } else if (arg == "-d" || arg == "--dir") {
      if (i + 1 < argc) {
        output_dir = argv[++i];
      }
    } else if (arg == "-p" || arg == "--prefix") {
      if (i + 1 < argc) {
        file_prefix = argv[++i];
      }
    }
  }

  // Create output directory if it doesn't exist
  std::filesystem::create_directories(output_dir);

  // Print configuration
  std::cout << "=== DELILA2 FileWriter ===" << std::endl;
  std::cout << "Input address:   " << input_address << std::endl;
  std::cout << "Output directory:" << output_dir << std::endl;
  std::cout << "File prefix:     " << file_prefix << std::endl;
  std::cout << std::endl;

  // Setup signal handlers
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  // Create and configure writer
  FileWriter writer;
  g_writer = &writer;

  writer.SetComponentId("writer");
  writer.SetInputAddresses({input_address});
  writer.SetOutputPath(output_dir);
  writer.SetFilePrefix(file_prefix);

  // Initialize
  std::cout << "Initializing writer..." << std::endl;
  if (!writer.Initialize("")) {
    std::cerr << "ERROR: Failed to initialize writer" << std::endl;
    return 1;
  }

  // Arm
  std::cout << "Arming writer..." << std::endl;
  if (!writer.Arm()) {
    std::cerr << "ERROR: Failed to arm writer" << std::endl;
    return 1;
  }

  // Start with run number 1
  std::cout << "Starting writer (Run 1)..." << std::endl;
  if (!writer.Start(1)) {
    std::cerr << "ERROR: Failed to start writer" << std::endl;
    return 1;
  }

  std::cout << "Writer running. Press Ctrl+C to stop." << std::endl;

  // Main loop - print status periodically
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (g_running) {
      auto status = writer.GetStatus();
      std::cout << "[Status] Events: " << status.metrics.events_processed
                << ", Bytes: " << status.metrics.bytes_transferred << std::endl;
    }
  }

  // Cleanup
  std::cout << "Stopping writer..." << std::endl;
  writer.Stop(true);
  writer.Shutdown();

  auto status = writer.GetStatus();
  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events:     " << status.metrics.events_processed << std::endl;
  std::cout << "Total bytes:      " << status.metrics.bytes_transferred << std::endl;

  g_writer = nullptr;
  return 0;
}
