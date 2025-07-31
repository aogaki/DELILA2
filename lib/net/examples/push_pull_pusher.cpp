#include <iostream>
#include <thread>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // Configure pusher
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5556";
    config.data_pattern = "PUSH";
    config.bind_data = true;
    
    // Set status and command addresses same as data to disable separate channels
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
    
    std::cout << "Pusher started, distributing work..." << std::endl;
    
    for (int batch = 0; batch < 1000; ++batch) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        // Create work batch
        for (int i = 0; i < 100; ++i) {
            auto event = std::make_unique<EventData>();
            event->energy = batch * 100 + i;
            event->module = batch % 4;
            events->push_back(std::move(event));
        }
        
        transport.Send(events);
        std::cout << "Sent batch " << batch << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}