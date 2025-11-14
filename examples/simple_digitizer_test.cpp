// Simple example demonstrating PSD2 digitizer access and data acquisition
//
// This example shows:
// - Loading configuration from a file
// - Initializing and configuring a digitizer
// - Starting data acquisition
// - Fetching event data in a loop
// - Displaying statistics
// - Clean shutdown
//
// Usage: ./simple_digitizer_test [config_file]
// Default config file: PSD2.conf

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

#include <delila/digitizer/Digitizer.hpp>
#include <delila/digitizer/ConfigurationManager.hpp>

using DELILA::Digitizer::ConfigurationManager;
using DELILA::Digitizer::Digitizer;

// Simple keyboard input check (non-blocking)
bool CheckForQuit()
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

  return (result > 0 && (input == 'q' || input == 'Q'));
}

int main(int argc, char* argv[])
{
  std::cout << "=== DELILA2 Simple Digitizer Test ===" << std::endl;
  std::cout << "Press 'q' to quit" << std::endl << std::endl;

  // Parse command line arguments
  std::string config_file = "PSD2.conf";
  if (argc > 1) {
    config_file = argv[1];
  }

  std::cout << "Configuration file: " << config_file << std::endl;

  // Initialize digitizer
  auto digitizer = std::make_unique<Digitizer>();
  auto configManager = ConfigurationManager();

  try {
    std::cout << "Loading configuration..." << std::endl;
    configManager.LoadFromFile(config_file);

    std::cout << "Initializing digitizer..." << std::endl;
    digitizer->Initialize(configManager);

    std::cout << "Configuring digitizer..." << std::endl;
    digitizer->Configure();

    std::cout << "Configuration complete!" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Failed to initialize digitizer: " << e.what() << std::endl;
    return 1;
  }

  // Start data acquisition
  std::cout << "\nStarting acquisition..." << std::endl;
  digitizer->StartAcquisition();

  // Statistics counters
  auto startTime = std::chrono::steady_clock::now();
  uint64_t totalEvents = 0;
  uint64_t totalBatches = 0;
  uint64_t emptyBatches = 0;

  auto lastStatsTime = startTime;
  uint64_t lastEventCount = 0;

  // Main acquisition loop
  bool isAcquiring = true;
  while (isAcquiring) {
    // Check for quit command
    if (CheckForQuit()) {
      std::cout << "\nQuit requested..." << std::endl;
      break;
    }

    // Get event data from digitizer
    auto events = digitizer->GetEventData();

    if (events && !events->empty()) {
      totalEvents += events->size();
      totalBatches++;

      // Display information about the last event in this batch
      const auto& lastEvent = events->back();
      std::cout << "Batch #" << totalBatches
                << " | Events: " << events->size()
                << " | Ch: " << static_cast<int>(lastEvent->channel)
                << " | Timestamp: " << lastEvent->timeStampNs
                << " ns" << std::endl;
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

      std::cout << "\n--- Statistics ---" << std::endl;
      std::cout << "Total events: " << totalEvents << std::endl;
      std::cout << "Event rate: " << std::fixed << std::setprecision(1)
                << rate << " evt/s" << std::endl;
      std::cout << "Total batches: " << totalBatches << std::endl;
      std::cout << "------------------\n" << std::endl;

      lastStatsTime = currentTime;
      lastEventCount = totalEvents;
    }
  }

  // Stop acquisition
  std::cout << "\nStopping acquisition..." << std::endl;
  digitizer->StopAcquisition();

  // Final statistics
  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;

  std::cout << "\n=== Final Statistics ===" << std::endl;
  std::cout << "Total events: " << totalEvents << std::endl;
  std::cout << "Total batches: " << totalBatches << std::endl;
  std::cout << "Empty batches: " << emptyBatches << std::endl;
  std::cout << "Elapsed time: " << std::fixed << std::setprecision(2)
            << elapsedTime.count() << " seconds" << std::endl;

  if (elapsedTime.count() > 0) {
    std::cout << "Average rate: " << std::fixed << std::setprecision(1)
              << totalEvents / elapsedTime.count() << " events/second" << std::endl;
  }

  if (totalBatches > 0) {
    std::cout << "Average batch size: " << std::fixed << std::setprecision(1)
              << static_cast<double>(totalEvents) / totalBatches << " events/batch" << std::endl;
  }

  std::cout << "\nDone!" << std::endl;
  return 0;
}
