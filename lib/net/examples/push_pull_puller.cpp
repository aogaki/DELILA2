#include <iostream>
#include <thread>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main(int argc, char* argv[]) {
    int worker_id = argc > 1 ? std::stoi(argv[1]) : 0;
    
    // Configure puller
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5556";
    config.data_pattern = "PULL";
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
    
    std::cout << "Worker " << worker_id << " started, waiting for work..." << std::endl;
    
    while (true) {
        auto [events, sequence] = transport.Receive();
        
        if (events) {
            // Process events
            std::cout << "Worker " << worker_id << " processing " 
                      << events->size() << " events from batch " << sequence << std::endl;
            
            // Simulate processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    return 0;
}