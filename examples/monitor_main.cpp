/**
 * @file monitor_main.cpp
 * @brief MonitorROOT executable
 *
 * Receives data from upstream and displays histograms via THttpServer.
 * Access the web interface at http://localhost:<port>
 *
 * Usage:
 *   delila_monitor [options]
 *
 * Options:
 *   -i, --input <address>    ZMQ input address (default: tcp://localhost:5560)
 *   -p, --port <number>      HTTP server port (default: 8080)
 *   --no-energy              Disable energy histogram
 *   --no-channel             Disable channel histogram
 *   --timing                 Enable timing histogram
 *   --2d                     Enable 2D histograms (Energy vs Channel/Module)
 *   --waveform <mod,ch>      Enable waveform display for specified module,channel
 *   -h, --help               Show this help message
 *
 * Example:
 *   # Monitor with default histograms on port 8080
 *   delila_monitor -i tcp://localhost:5560 -p 8080
 *
 *   # Monitor with 2D histograms and waveform display
 *   delila_monitor -i tcp://localhost:5560 --2d --waveform 0,0
 *
 * Web Interface:
 *   Open http://localhost:8080 in a web browser to view histograms.
 *   JSROOT provides interactive visualization.
 */

#ifdef HAS_ROOT

#include <MonitorROOT.hpp>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

using namespace DELILA;

// Global pointer for signal handler
static MonitorROOT* g_monitor = nullptr;
static volatile bool g_running = true;

void signalHandler(int signum) {
  std::cout << "\nReceived signal " << signum << ", shutting down..."
            << std::endl;
  g_running = false;
  if (g_monitor) {
    g_monitor->Stop(true);
  }
}

void printUsage(const char* program) {
  std::cout << "DELILA2 MonitorROOT - Online Histogram Display\n\n";
  std::cout << "Usage: " << program << " [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -i, --input <address>    ZMQ input address (default: tcp://localhost:5560)\n";
  std::cout << "  -p, --port <number>      HTTP server port (default: 8080)\n";
  std::cout << "  --no-energy              Disable energy histogram\n";
  std::cout << "  --no-channel             Disable channel histogram\n";
  std::cout << "  --timing                 Enable timing histogram\n";
  std::cout << "  --2d                     Enable 2D histograms\n";
  std::cout << "  --waveform <mod,ch>      Enable waveform display\n";
  std::cout << "  -h, --help               Show this help message\n\n";
  std::cout << "Example:\n";
  std::cout << "  " << program << " -i tcp://localhost:5560 -p 8080 --2d\n\n";
  std::cout << "Web Interface:\n";
  std::cout << "  Open http://localhost:<port> in a web browser.\n";
}

int main(int argc, char* argv[]) {
  // Default configuration
  std::string input_address = "tcp://localhost:5560";
  int http_port = 8080;
  bool enable_energy = true;
  bool enable_channel = true;
  bool enable_timing = false;
  bool enable_2d = false;
  bool enable_waveform = false;
  uint8_t waveform_module = 0;
  uint8_t waveform_channel = 0;

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
    } else if (arg == "-p" || arg == "--port") {
      if (i + 1 < argc) {
        http_port = std::stoi(argv[++i]);
      }
    } else if (arg == "--no-energy") {
      enable_energy = false;
    } else if (arg == "--no-channel") {
      enable_channel = false;
    } else if (arg == "--timing") {
      enable_timing = true;
    } else if (arg == "--2d") {
      enable_2d = true;
    } else if (arg == "--waveform") {
      if (i + 1 < argc) {
        enable_waveform = true;
        std::string spec = argv[++i];
        auto comma = spec.find(',');
        if (comma != std::string::npos) {
          waveform_module = static_cast<uint8_t>(std::stoi(spec.substr(0, comma)));
          waveform_channel = static_cast<uint8_t>(std::stoi(spec.substr(comma + 1)));
        }
      }
    }
  }

  // Print configuration
  std::cout << "=== DELILA2 MonitorROOT ===" << std::endl;
  std::cout << "Input address:   " << input_address << std::endl;
  std::cout << "HTTP port:       " << http_port << std::endl;
  std::cout << "Histograms:" << std::endl;
  std::cout << "  Energy:        " << (enable_energy ? "enabled" : "disabled") << std::endl;
  std::cout << "  Channel:       " << (enable_channel ? "enabled" : "disabled") << std::endl;
  std::cout << "  Timing:        " << (enable_timing ? "enabled" : "disabled") << std::endl;
  std::cout << "  2D:            " << (enable_2d ? "enabled" : "disabled") << std::endl;
  if (enable_waveform) {
    std::cout << "  Waveform:      module " << static_cast<int>(waveform_module)
              << ", channel " << static_cast<int>(waveform_channel) << std::endl;
  }
  std::cout << std::endl;

  // Setup signal handlers
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  // Create and configure monitor
  MonitorROOT monitor;
  g_monitor = &monitor;

  monitor.SetComponentId("monitor");
  monitor.SetInputAddresses({input_address});
  monitor.SetHttpPort(http_port);
  monitor.EnableEnergyHistogram(enable_energy);
  monitor.EnableChannelHistogram(enable_channel);
  monitor.EnableTimingHistogram(enable_timing);
  monitor.Enable2DHistogram(enable_2d);
  monitor.EnableWaveformDisplay(enable_waveform);
  if (enable_waveform) {
    monitor.SetWaveformChannel(waveform_module, waveform_channel);
  }

  // Initialize
  std::cout << "Initializing monitor..." << std::endl;
  if (!monitor.Initialize("")) {
    std::cerr << "ERROR: Failed to initialize monitor" << std::endl;
    return 1;
  }

  // Arm
  std::cout << "Arming monitor..." << std::endl;
  if (!monitor.Arm()) {
    std::cerr << "ERROR: Failed to arm monitor" << std::endl;
    return 1;
  }

  // Start with run number 1
  std::cout << "Starting monitor (Run 1)..." << std::endl;
  if (!monitor.Start(1)) {
    std::cerr << "ERROR: Failed to start monitor" << std::endl;
    return 1;
  }

  std::cout << "\n*** Monitor running ***" << std::endl;
  std::cout << "Open http://localhost:" << http_port << " in a web browser." << std::endl;
  std::cout << "Press Ctrl+C to stop.\n" << std::endl;

  // Main loop - print status periodically
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (g_running) {
      auto status = monitor.GetStatus();
      std::cout << "[Status] Events: " << status.metrics.events_processed
                << ", Bytes: " << status.metrics.bytes_transferred << std::endl;
    }
  }

  // Cleanup
  std::cout << "Stopping monitor..." << std::endl;
  monitor.Stop(true);
  monitor.Shutdown();

  auto status = monitor.GetStatus();
  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events:     " << status.metrics.events_processed << std::endl;
  std::cout << "Total bytes:      " << status.metrics.bytes_transferred << std::endl;

  g_monitor = nullptr;
  return 0;
}

#else  // !HAS_ROOT

#include <iostream>

int main() {
  std::cerr << "ERROR: MonitorROOT requires ROOT library." << std::endl;
  std::cerr << "Please rebuild DELILA2 with ROOT installed." << std::endl;
  return 1;
}

#endif  // HAS_ROOT
