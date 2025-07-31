#include <iostream>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // Configure publisher
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5555";
    config.data_pattern = "PUB";
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
    
    std::cout << "Publisher started, sending events..." << std::endl;
    
    // Send events continuously
    uint64_t event_number = 0;
    while (true) {
        // Create batch of events
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int i = 0; i < 10; ++i) {
            auto event = std::make_unique<EventData>(512);
            event->timeStampNs = std::chrono::system_clock::now().time_since_epoch().count();
            event->energy = 1000 + (event_number % 2000);
            event->module = 0;
            event->channel = i;
            
            // Fill some waveform data
            for (size_t j = 0; j < 512; ++j) {
                event->analogProbe1[j] = static_cast<int32_t>(1000 * sin(j * 0.1));
            }
            
            events->push_back(std::move(event));
            event_number++;
        }
        
        if (!transport.Send(events)) {
            std::cerr << "Failed to send events" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}