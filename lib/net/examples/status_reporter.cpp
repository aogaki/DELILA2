#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    // Configure status publisher
    ZMQTransport transport;
    TransportConfig config;
    config.status_address = "tcp://*:5556";
    config.bind_status = true;
    
    // Set data and command addresses to be different
    config.data_address = "tcp://localhost:9994";
    config.command_address = "tcp://localhost:9995";
    config.data_pattern = "SUB";
    config.bind_data = false;
    config.bind_command = false;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "Status reporter started..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> temp_dist(20.0, 30.0);
    std::uniform_real_distribution<> rate_dist(0.8, 1.2);
    
    uint64_t heartbeat = 0;
    
    while (true) {
        ComponentStatus status;
        status.component_id = "digitizer_01";
        status.state = "RUNNING";
        status.timestamp = std::chrono::system_clock::now();
        status.heartbeat_counter = heartbeat++;
        
        // Add metrics
        status.metrics["temperature_c"] = temp_dist(gen);
        status.metrics["event_rate_hz"] = 1000000 * rate_dist(gen);
        status.metrics["buffer_usage_percent"] = 25.5 + (heartbeat % 50);
        status.metrics["dropped_events"] = 0;
        
        if (!transport.SendStatus(status)) {
            std::cerr << "Failed to send status" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}