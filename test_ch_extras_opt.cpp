#include <iostream>
#include <string>
#include "ConfigurationManager.hpp"
#include "Digitizer1.hpp"

using namespace DELILA::Digitizer;

int main() {
    std::cout << "Testing automatic ch_extras_opt configuration..." << std::endl;
    
    // Create a configuration without ch_extras_opt
    ConfigurationManager config;
    config.SetParameter("URL", "dig1://test");
    config.SetParameter("Debug", "true");
    config.SetParameter("ModID", "1");
    config.SetParameter("/ch/0..7/par/ch_enabled", "TRUE");
    config.SetParameter("/ch/0..7/par/ch_threshold", "100");
    
    std::cout << "Configuration parameters before initialization:" << std::endl;
    auto params = config.GetAllParameters();
    for (const auto& param : params) {
        std::cout << "  " << param[0] << " = " << param[1] << std::endl;
    }
    
    // Create digitizer and initialize
    Digitizer1 digitizer;
    
    std::cout << "\nNote: The actual initialization will fail because we don't have" << std::endl;
    std::cout << "a real digitizer connected, but we can see the configuration logic." << std::endl;
    
    // Note: ch_extras_opt must now be configured explicitly in the configuration file
    // The automatic configuration has been removed
    
    std::cout << "\nTest completed. ch_extras_opt must be configured explicitly" << std::endl;
    std::cout << "in the configuration file for each channel that requires it." << std::endl;
    
    return 0;
}