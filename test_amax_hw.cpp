/**
 * @file test_amax_hw.cpp
 * @brief Minimal hardware access test for AMax firmware using configuration file
 *
 * Simple test program that:
 * 1. Opens connection to digitizer
 * 2. Configures using AMax_test.conf
 * 3. Takes data for a short time
 * 4. Closes connection
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "lib/digitizer/include/AMaxDecoder.hpp"
#include "lib/digitizer/include/ConfigurationManager.hpp"
#include "lib/digitizer/include/Digitizer2.hpp"

using namespace DELILA::Digitizer;

// Global flag for clean shutdown
std::atomic<bool> g_stopRequested(false);

void signalHandler(int signal)
{
  std::cout << "\nInterrupt received, stopping acquisition..." << std::endl;
  g_stopRequested = true;
}

int main(int argc, char *argv[])
{
  // Set up signal handler for clean exit
  std::signal(SIGINT, signalHandler);

  std::cout << "==================================" << std::endl;
  std::cout << "  AMax Hardware Access Test" << std::endl;
  std::cout << "==================================" << std::endl;

  // Configuration file path
  std::string configFile = "AMax_test.conf";
  if (argc > 1) {
    configFile = argv[1];
  }
  std::cout << "Using configuration file: " << configFile << std::endl;

  // Data acquisition duration
  int duration = 5;  // seconds
  if (argc > 2) {
    duration = std::atoi(argv[2]);
  }

  try {
    std::cout << "\n1. Initializing digitizer..." << std::endl;

    auto digitizer = std::make_unique<Digitizer2>();

    // Create configuration manager and load config file
    ConfigurationManager configManager;
    auto loadResult = configManager.LoadFromFile(configFile);
    if (loadResult != ConfigurationManager::LoadResult::Success) {
      std::cerr << "Failed to load configuration from " << configFile
                << std::endl;
      std::cerr << "Error: " << configManager.GetLastError() << std::endl;
      return 1;
    }
    std::cout << "   ✓ Configuration file loaded" << std::endl;

    // Initialize with configuration
    if (!digitizer->Initialize(configManager)) {
      std::cerr << "Failed to initialize digitizer" << std::endl;
      return 1;
    }
    std::cout << "   ✓ Digitizer initialized successfully" << std::endl;

    // Create decoder for data analysis
    auto decoder = std::make_unique<AMaxDecoder>();
    decoder->SetDumpFlag(false);  // Enable dumping for analysis
    decoder->SetModuleNumber(digitizer->GetModuleNumber());
    decoder->SetTimeStep(2);  // 2ns time step (typical for V2730)

    // Print device info to verify AMax firmware
    digitizer->PrintDeviceInfo();

    std::cout << "\n2. Configuring digitizer..." << std::endl;

    // Configure digitizer (this applies the settings from the config file)
    if (!digitizer->Configure()) {
      std::cerr << "Failed to configure digitizer" << std::endl;
      return 1;
    }

    std::cout << "   ✓ Configuration applied" << std::endl;

    std::cout << "\n3. Starting data acquisition..." << std::endl;

    // Start acquisition
    if (!digitizer->StartAcquisition()) {
      std::cerr << "Failed to start acquisition" << std::endl;
      return 1;
    }
    std::cout << "   ✓ Acquisition started" << std::endl;

    // Get event data
    for (auto i = 0; i < 1; ++i) digitizer->SendSWTrigger();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto eventData = digitizer->GetEventData();
    std::cout << "   ✓ Retrieved " << eventData->size()
              << " events after 1 second" << std::endl;

    // Stop acquisition
    std::cout << "\n   - Stopping acquisition..." << std::endl;
    if (!digitizer->StopAcquisition()) {
      std::cerr << "Warning: Failed to stop acquisition cleanly" << std::endl;
    }

    // ====================
    // 4. CLEANUP
    // ====================
    std::cout << "\n4. Cleanup..." << std::endl;

    // Digitizer destructor will handle cleanup
    digitizer.reset();
    std::cout << "   ✓ Cleanup complete" << std::endl;

    std::cout << "\n==================================" << std::endl;
    std::cout << "  Test completed successfully!" << std::endl;
    std::cout << "==================================" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "\nError: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}