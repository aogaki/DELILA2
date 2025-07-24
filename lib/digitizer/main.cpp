#include <TApplication.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "ConfigurationManager.hpp"
#include "Digitizer.hpp"

using namespace DELILA::Digitizer;

char getKey()
{
  struct termios oldt, newt;
  int oldf;
  char ch;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  return ch;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file> [--save-tree-only]" << std::endl;
    std::cerr << "  --save-tree-only: Save device tree and exit (no acquisition)" << std::endl;
    return 1;
  }
  
  bool saveTreeOnly = false;
  if (argc > 2 && std::string(argv[2]) == "--save-tree-only") {
    saveTreeOnly = true;
  }

  TApplication app("app", 0, nullptr);

  ConfigurationManager config;
  if (config.LoadFromFile(argv[1]) !=
      ConfigurationManager::LoadResult::Success) {
    std::cerr << "Failed to load configuration: " << config.GetLastError()
              << std::endl;
    return 1;
  }

  Digitizer digitizer;
  if (!digitizer.Initialize(config)) {
    std::cerr << "Failed to initialize digitizer" << std::endl;
    return 1;
  }

  digitizer.PrintDeviceInfo();

  // Save device tree with firmware type detection BEFORE configuration and validation
  const auto &deviceTree = digitizer.GetDeviceTreeJSON();
  if (!deviceTree.empty()) {
    std::string filename = "devTree.json";
    std::string configFile = argv[1];
    
    // Extract firmware type and model from device tree
    std::string fwType = "unknown";
    std::string modelName = "unknown";
    
    if (deviceTree.contains("par")) {
      if (deviceTree["par"].contains("fwtype")) {
        fwType = deviceTree["par"]["fwtype"]["value"];
      }
      if (deviceTree["par"].contains("modelname")) {
        modelName = deviceTree["par"]["modelname"]["value"];
      }
    }
    
    // Generate filename based on firmware type and model
    if (fwType != "unknown") {
      filename = "devTree_" + fwType + "_" + modelName + ".json";
    } else {
      // Fallback to config file name detection
      if (configFile.find("PSD1") != std::string::npos) {
        filename = "devTree_PSD1.json";
      } else if (configFile.find("PSD2") != std::string::npos) {
        filename = "devTree_PSD2.json";
      } else if (configFile.find("PHA1") != std::string::npos) {
        filename = "devTree_PHA1.json";
      } else if (configFile.find("dig1") != std::string::npos) {
        filename = "devTree_dig1.json";
      } else if (configFile.find("dig2") != std::string::npos) {
        filename = "devTree_dig2.json";
      }
    }

    std::ofstream file(filename);
    if (file.is_open()) {
      file << deviceTree.dump(2);
      std::cout << "Device tree saved to " << filename << std::endl;
      std::cout << "Firmware Type: " << fwType << ", Model: " << modelName << std::endl;
    } else {
      std::cerr << "Failed to save device tree to " << filename << std::endl;
    }
  }

  if (!digitizer.Configure()) {
    std::cerr << "Failed to configure digitizer" << std::endl;
    return 1;
  }

  std::cout << "Digitizer ready! Press 'q' to quit." << std::endl;

  digitizer.StartAcquisition();
  // Main loop
  double eventCounter = 0;
  auto startTime = std::chrono::system_clock::now();

  while (true) {
    char key = getKey();
    if (key == 'q' || key == 'Q') break;

    auto eventData = digitizer.GetEventData();
    if (eventData->size() > 0) {
      eventCounter += eventData->size();
      std::cout << eventData->back()->timeStampNs << " Received "
                << eventData->size()
                << " events (Total: " << (size_t)eventCounter << ")"
                << std::endl;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  // Statistics
  auto endTime = std::chrono::system_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime)
          .count();

  std::cout << "\n=== STATISTICS ===" << std::endl;
  std::cout << "Duration: " << std::fixed << std::setprecision(3)
            << duration / 1000.0 << " seconds" << std::endl;
  std::cout << "Events: " << (size_t)eventCounter << std::endl;
  std::cout << "Rate: " << std::fixed << std::setprecision(1)
            << eventCounter / duration * 1000 << " Hz" << std::endl;

  digitizer.StopAcquisition();
  return 0;
}
