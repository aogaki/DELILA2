/**
 * @file emulator_main.cpp
 * @brief Digitizer emulator executable
 *
 * Generates synthetic event data and sends via ZMQ.
 * Can be used for testing pipelines without real hardware.
 *
 * Usage:
 *   delila_emulator [options]
 *
 * Options:
 *   -o, --output <address>   ZMQ output address (default: tcp://*:5555)
 *   -m, --module <number>    Module number 0-255 (default: 0)
 *   -c, --channels <number>  Number of channels 1-64 (default: 16)
 *   -r, --rate <events/sec>  Event generation rate (default: 1000)
 *   -e, --energy <min,max>   Energy range (default: 0,16383)
 *   --full                   Use full EventData mode (default: Minimal)
 *   --waveform <size>        Waveform samples (Full mode only, default: 0)
 *   --seed <value>           Random seed for reproducibility
 *   -h, --help               Show this help message
 *
 * Example:
 *   # Start emulator for module 0, output on port 5555
 *   delila_emulator -o tcp://*:5555 -m 0 -r 10000
 *
 *   # Start emulator with full waveform data
 *   delila_emulator -o tcp://*:5556 -m 1 --full --waveform 1024
 */

#include <Emulator.hpp>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

using namespace DELILA;

// Global pointer for signal handler
static Emulator* g_emulator = nullptr;
static volatile bool g_running = true;

void signalHandler(int signum) {
  std::cout << "\nReceived signal " << signum << ", shutting down..."
            << std::endl;
  g_running = false;
  if (g_emulator) {
    g_emulator->Stop(true);
  }
}

void printUsage(const char* program) {
  std::cout << "DELILA2 Emulator - Synthetic Event Data Generator\n\n";
  std::cout << "Usage: " << program << " [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -o, --output <address>   ZMQ output address (default: tcp://*:5555)\n";
  std::cout << "  -m, --module <number>    Module number 0-255 (default: 0)\n";
  std::cout << "  -c, --channels <number>  Number of channels 1-64 (default: 16)\n";
  std::cout << "  -r, --rate <events/sec>  Event generation rate (default: 1000)\n";
  std::cout << "  -e, --energy <min,max>   Energy range (default: 0,16383)\n";
  std::cout << "  --full                   Use full EventData mode (default: Minimal)\n";
  std::cout << "  --waveform <size>        Waveform samples (Full mode, default: 0)\n";
  std::cout << "  --seed <value>           Random seed for reproducibility\n";
  std::cout << "  -h, --help               Show this help message\n\n";
  std::cout << "Example:\n";
  std::cout << "  " << program << " -o tcp://*:5555 -m 0 -r 10000\n";
}

int main(int argc, char* argv[]) {
  // Default configuration
  std::string output_address = "tcp://*:5555";
  uint8_t module_number = 0;
  uint8_t num_channels = 16;
  uint32_t event_rate = 1000;
  uint16_t energy_min = 0;
  uint16_t energy_max = 16383;
  EmulatorDataMode data_mode = EmulatorDataMode::Minimal;
  size_t waveform_size = 0;
  bool seed_set = false;
  uint64_t seed = 0;

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 < argc) {
        output_address = argv[++i];
      }
    } else if (arg == "-m" || arg == "--module") {
      if (i + 1 < argc) {
        module_number = static_cast<uint8_t>(std::stoi(argv[++i]));
      }
    } else if (arg == "-c" || arg == "--channels") {
      if (i + 1 < argc) {
        num_channels = static_cast<uint8_t>(std::stoi(argv[++i]));
      }
    } else if (arg == "-r" || arg == "--rate") {
      if (i + 1 < argc) {
        event_rate = static_cast<uint32_t>(std::stoi(argv[++i]));
      }
    } else if (arg == "-e" || arg == "--energy") {
      if (i + 1 < argc) {
        std::string range = argv[++i];
        auto comma = range.find(',');
        if (comma != std::string::npos) {
          energy_min = static_cast<uint16_t>(std::stoi(range.substr(0, comma)));
          energy_max = static_cast<uint16_t>(std::stoi(range.substr(comma + 1)));
        }
      }
    } else if (arg == "--full") {
      data_mode = EmulatorDataMode::Full;
    } else if (arg == "--waveform") {
      if (i + 1 < argc) {
        waveform_size = static_cast<size_t>(std::stoi(argv[++i]));
      }
    } else if (arg == "--seed") {
      if (i + 1 < argc) {
        seed = static_cast<uint64_t>(std::stoull(argv[++i]));
        seed_set = true;
      }
    }
  }

  // Print configuration
  std::cout << "=== DELILA2 Emulator ===" << std::endl;
  std::cout << "Output address: " << output_address << std::endl;
  std::cout << "Module number:  " << static_cast<int>(module_number) << std::endl;
  std::cout << "Channels:       " << static_cast<int>(num_channels) << std::endl;
  std::cout << "Event rate:     " << event_rate << " events/sec" << std::endl;
  std::cout << "Energy range:   " << energy_min << " - " << energy_max << std::endl;
  std::cout << "Data mode:      " << (data_mode == EmulatorDataMode::Minimal ? "Minimal" : "Full") << std::endl;
  if (data_mode == EmulatorDataMode::Full) {
    std::cout << "Waveform size:  " << waveform_size << " samples" << std::endl;
  }
  std::cout << std::endl;

  // Setup signal handlers
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  // Create and configure emulator
  Emulator emulator;
  g_emulator = &emulator;

  emulator.SetComponentId("emulator_mod" + std::to_string(module_number));
  emulator.SetModuleNumber(module_number);
  emulator.SetNumChannels(num_channels);
  emulator.SetEventRate(event_rate);
  emulator.SetEnergyRange(energy_min, energy_max);
  emulator.SetDataMode(data_mode);
  emulator.SetWaveformSize(waveform_size);
  emulator.SetOutputAddresses({output_address});

  if (seed_set) {
    emulator.SetSeed(seed);
  }

  // Initialize
  std::cout << "Initializing emulator..." << std::endl;
  if (!emulator.Initialize("")) {
    std::cerr << "ERROR: Failed to initialize emulator" << std::endl;
    return 1;
  }

  // Arm
  std::cout << "Arming emulator..." << std::endl;
  if (!emulator.Arm()) {
    std::cerr << "ERROR: Failed to arm emulator" << std::endl;
    return 1;
  }

  // Start with run number 1
  std::cout << "Starting emulator (Run 1)..." << std::endl;
  if (!emulator.Start(1)) {
    std::cerr << "ERROR: Failed to start emulator" << std::endl;
    return 1;
  }

  std::cout << "Emulator running. Press Ctrl+C to stop." << std::endl;

  // Main loop - print status periodically
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (g_running) {
      auto status = emulator.GetStatus();
      std::cout << "[Status] Events: " << status.metrics.events_processed
                << ", Bytes: " << status.metrics.bytes_transferred << std::endl;
    }
  }

  // Cleanup
  std::cout << "Stopping emulator..." << std::endl;
  emulator.Stop(true);
  emulator.Shutdown();

  auto status = emulator.GetStatus();
  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events:     " << status.metrics.events_processed << std::endl;
  std::cout << "Total bytes:      " << status.metrics.bytes_transferred << std::endl;

  g_emulator = nullptr;
  return 0;
}
