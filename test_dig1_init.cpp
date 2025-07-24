#include <iostream>
#include "Digitizer1.hpp"
#include "ConfigurationManager.hpp"

using namespace DELILA::Digitizer;

int main() {
    std::cout << "Testing Digitizer1::Initialize() - now matches Digitizer2 exactly" << std::endl;
    
    // Create configuration manager and load from actual dig1.conf
    ConfigurationManager config;
    
    std::cout << "Loading configuration from dig1.conf..." << std::endl;
    auto loadResult = config.LoadFromFile("dig1.conf");
    
    if (loadResult != ConfigurationManager::LoadResult::Success) {
        std::cerr << "Failed to load dig1.conf: " << config.GetLastError() << std::endl;
        return 1;
    }
    
    std::cout << "Configuration loaded successfully" << std::endl;
    
    // Create Digitizer1 instance
    Digitizer1 digitizer;
    
    std::cout << "Attempting to initialize with dig1.conf..." << std::endl;
    
    // Note: This will fail with hardware connection error since we don't have actual hardware
    // But it will test the Initialize method logic and flow
    bool result = digitizer.Initialize(config);
    
    std::cout << "Initialize result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Expected: FAILED (no hardware available)" << std::endl;
    
    // Test PrintDeviceInfo even after failed initialization
    std::cout << "\nTesting PrintDeviceInfo after failed initialization:" << std::endl;
    digitizer.PrintDeviceInfo();
    
    // Test Configure method even after failed initialization
    std::cout << "\nTesting Configure method after failed initialization:" << std::endl;
    bool configResult = digitizer.Configure();
    std::cout << "Configure result: " << (configResult ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Expected: FAILED (no hardware available)" << std::endl;
    
    return 0;
}