#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include "ZMQTransport.hpp"
#include "nlohmann/json.hpp"

using namespace DELILA::Net;
using json = nlohmann::json;

int main() {
    // Configure as publisher for status updates
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5556";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    // Disable status and command channels
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "Status reporter (PUB) started..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> temp_dist(20.0, 30.0);
    std::uniform_real_distribution<> rate_dist(0.8, 1.2);
    
    uint64_t heartbeat = 0;
    
    while (true) {
        // Create a fake event that contains status information
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        auto event = std::make_unique<EventData>();
        
        // Encode status info in event fields (this is a workaround)
        event->timeStampNs = std::chrono::system_clock::now().time_since_epoch().count();
        event->energy = static_cast<uint16_t>(temp_dist(gen) * 100); // Temperature * 100
        event->energyShort = static_cast<uint16_t>(1000000 * rate_dist(gen) / 1000); // Event rate / 1000
        event->module = 1;  // Status message indicator
        event->channel = heartbeat % 256;
        event->flags = heartbeat++;
        
        events->push_back(std::move(event));
        
        if (!transport.Send(events)) {
            std::cerr << "Failed to send status" << std::endl;
        } else {
            std::cout << "Sent status update " << heartbeat << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}