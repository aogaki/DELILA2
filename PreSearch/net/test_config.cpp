#include "Config.hpp"
#include <iostream>

int main() {
    using namespace delila::test;
    
    Config config;
    if (config.LoadFromFile("config/config.json")) {
        std::cout << "✓ Configuration loaded successfully" << std::endl;
        std::cout << "Protocol: " << config.GetProtocolString() << std::endl;
        std::cout << "Transport: " << config.GetTransportString() << std::endl;
        std::cout << "Duration: " << config.GetTestConfig().durationMinutes << " minutes" << std::endl;
        std::cout << "Batch sizes: ";
        for (auto size : config.GetTestConfig().batchSizes) {
            std::cout << size << " ";
        }
        std::cout << std::endl;
        std::cout << "Merger address: " << config.GetNetworkConfig().mergerAddress << std::endl;
        return 0;
    } else {
        std::cerr << "✗ Failed to load configuration" << std::endl;
        return 1;
    }
}