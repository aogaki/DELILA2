/**
 * @file digitizer2_basic_example.cpp
 * @brief Basic example demonstrating Digitizer2 access and usage
 *
 * This example shows:
 * - Creating and initializing a Digitizer2 instance
 * - Loading configuration from file
 * - Starting data acquisition
 * - Reading event data in a loop
 * - Proper shutdown sequence
 *
 * Usage: ./digitizer2_basic_example [config_file]
 * Default config file: dig2.conf
 *
 * @note This example assumes you have a CAEN Digitizer 2 connected
 *       and accessible via the network URL specified in the config file.
 */

#include <iostream>
#include <iomanip>
#include <memory>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

#include <delila/digitizer/Digitizer2.hpp>
#include <delila/digitizer/ConfigurationManager.hpp>

using namespace DELILA::Digitizer;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

/**
 * @brief Signal handler for graceful shutdown
 *
 * Handles SIGINT (Ctrl+C) to cleanly stop the acquisition loop.
 */
void SignalHandler(int signal)
{
  if (signal == SIGINT) {
    std::cout << "\n\nReceived SIGINT (Ctrl+C), stopping acquisition..." << std::endl;
    g_running = false;
  }
}

/**
 * @brief Display event information
 *
 * Prints detailed information about an event to the console.
 */
void DisplayEvent(const EventData& event)
{
  std::cout << std::fixed << std::setprecision(3);
  std::cout << "  Module: " << static_cast<int>(event.module)
            << " | Channel: " << static_cast<int>(event.channel)
            << " | Timestamp: " << event.timeStampNs << " ns"
            << " | Energy: " << event.energy
            << " | Short: " << event.energyShort
            << std::endl;
}

int main(int argc, char* argv[])
{
  std::cout << "========================================" << std::endl;
  std::cout << "  DELILA2 Digitizer2 Basic Example" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Press Ctrl+C to stop acquisition" << std::endl << std::endl;

  // Setup signal handler for graceful shutdown
  signal(SIGINT, SignalHandler);

  // Parse command line arguments
  std::string config_file = "dig2.conf";
  if (argc > 1) {
    config_file = argv[1];
  }
  std::cout << "Configuration file: " << config_file << std::endl;

  // Step 1: Create ConfigurationManager and load configuration
  std::cout << "\n[1] Loading configuration..." << std::endl;
  ConfigurationManager config;

  auto load_result = config.LoadFromFile(config_file);
  if (load_result != ConfigurationManager::LoadResult::Success) {
    std::cerr << "ERROR: Failed to load configuration: "
              << config.GetLastError() << std::endl;
    return 1;
  }
  std::cout << "    Configuration loaded successfully" << std::endl;

  // Step 2: Create Digitizer2 instance
  std::cout << "\n[2] Creating Digitizer2 instance..." << std::endl;
  auto digitizer = std::make_unique<Digitizer2>();
  std::cout << "    Digitizer2 instance created" << std::endl;

  // Step 3: Initialize the digitizer
  std::cout << "\n[3] Initializing digitizer..." << std::endl;
  if (!digitizer->Initialize(config)) {
    std::cerr << "ERROR: Failed to initialize digitizer" << std::endl;
    std::cerr << "       Make sure the digitizer is powered on and accessible" << std::endl;
    return 1;
  }
  std::cout << "    Digitizer initialized successfully" << std::endl;

  // Print device information
  std::cout << "\n[Device Information]" << std::endl;
  digitizer->PrintDeviceInfo();
  std::cout << "    Firmware Type: " << static_cast<int>(digitizer->GetType()) << std::endl;
  std::cout << "    Module Number: " << static_cast<int>(digitizer->GetModuleNumber()) << std::endl;

  // Step 4: Configure the digitizer
  std::cout << "\n[4] Configuring digitizer..." << std::endl;
  if (!digitizer->Configure()) {
    std::cerr << "ERROR: Failed to configure digitizer" << std::endl;
    return 1;
  }
  std::cout << "    Digitizer configured successfully" << std::endl;

  // Step 5: Start data acquisition
  std::cout << "\n[5] Starting data acquisition..." << std::endl;
  if (!digitizer->StartAcquisition()) {
    std::cerr << "ERROR: Failed to start acquisition" << std::endl;
    return 1;
  }
  std::cout << "    Acquisition started successfully" << std::endl;

  // Optional: Send a software trigger to test
  std::cout << "\n[6] Sending software trigger for testing..." << std::endl;
  if (digitizer->SendSWTrigger()) {
    std::cout << "    Software trigger sent" << std::endl;
  } else {
    std::cout << "    WARNING: Failed to send software trigger" << std::endl;
  }

  // Step 6: Main acquisition loop
  std::cout << "\n[7] Starting main acquisition loop..." << std::endl;
  std::cout << "    (Waiting for events...)\n" << std::endl;

  uint64_t total_events = 0;
  uint64_t batch_count = 0;
  auto start_time = std::chrono::steady_clock::now();

  while (g_running) {
    // Get event data from digitizer
    auto events = digitizer->GetEventData();

    if (events && !events->empty()) {
      batch_count++;
      total_events += events->size();

      std::cout << "Batch #" << batch_count
                << " | Events: " << events->size() << std::endl;

      // Display details of first few events (limit to avoid cluttering output)
      size_t display_count = std::min(events->size(), size_t(3));
      for (size_t i = 0; i < display_count; ++i) {
        DisplayEvent(*(*events)[i]);
      }

      if (events->size() > display_count) {
        std::cout << "  ... and " << (events->size() - display_count)
                  << " more events" << std::endl;
      }
      std::cout << std::endl;

    } else {
      // No events available, small sleep to avoid busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Check digitizer status periodically (every ~1000 loops)
    if (batch_count % 1000 == 0 && batch_count > 0) {
      if (!digitizer->CheckStatus()) {
        std::cerr << "WARNING: Status check failed" << std::endl;
      }
    }
  }

  // Step 7: Stop acquisition
  std::cout << "\n[8] Stopping acquisition..." << std::endl;
  if (!digitizer->StopAcquisition()) {
    std::cerr << "WARNING: Failed to stop acquisition cleanly" << std::endl;
  } else {
    std::cout << "    Acquisition stopped successfully" << std::endl;
  }

  // Step 8: Display final statistics
  auto end_time = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

  std::cout << "\n========================================" << std::endl;
  std::cout << "  Final Statistics" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Total events:    " << total_events << std::endl;
  std::cout << "Total batches:   " << batch_count << std::endl;
  std::cout << "Elapsed time:    " << elapsed.count() << " seconds" << std::endl;

  if (elapsed.count() > 0) {
    std::cout << "Average rate:    " << std::fixed << std::setprecision(1)
              << (double)total_events / elapsed.count() << " events/second" << std::endl;
  }

  if (batch_count > 0) {
    std::cout << "Avg batch size:  " << std::fixed << std::setprecision(1)
              << (double)total_events / batch_count << " events/batch" << std::endl;
  }

  std::cout << "\nExample completed successfully!" << std::endl;
  return 0;
}
