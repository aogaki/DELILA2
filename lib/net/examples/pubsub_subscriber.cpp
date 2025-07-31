#include <iostream>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // Configure subscriber
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5555";
    config.data_pattern = "SUB";
    config.bind_data = false;
    
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
    
    std::cout << "Subscriber started, waiting for events..." << std::endl;
    
    uint64_t total_events = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto [events, sequence] = transport.Receive();
        
        if (events) {
            total_events += events->size();
            
            // Print first event details
            if (!events->empty()) {
                const auto& first_event = (*events)[0];
                std::cout << "Received " << events->size() << " events, sequence: " << sequence
                          << ", first event - energy: " << first_event->energy
                          << ", channel: " << static_cast<int>(first_event->channel) << std::endl;
            }
            
            // Calculate rate every 100 batches
            if (sequence % 100 == 0) {
                auto current_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
                double rate = total_events / (duration.count() + 1.0);
                std::cout << "Event rate: " << rate << " events/sec" << std::endl;
            }
        }
    }
    
    return 0;
}