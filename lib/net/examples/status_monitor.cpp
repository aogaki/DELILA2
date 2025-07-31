#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <map>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    // Configure status subscriber
    ZMQTransport transport;
    TransportConfig config;
    config.status_address = "tcp://localhost:5556";
    config.bind_status = false;
    
    // Set data and command addresses to be different
    config.data_address = "tcp://localhost:9992";
    config.command_address = "tcp://localhost:9993";
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
    
    std::cout << "Status monitor started..." << std::endl;
    
    std::map<std::string, ComponentStatus> component_status;
    
    while (true) {
        auto status = transport.ReceiveStatus();
        
        if (status) {
            component_status[status->component_id] = *status;
            
            // Clear screen and display all component statuses
            std::cout << "\033[2J\033[H"; // Clear screen
            std::cout << "=== System Status Monitor ===" << std::endl;
            std::cout << std::endl;
            
            for (const auto& [id, st] : component_status) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - st.timestamp
                ).count();
                
                std::cout << "Component: " << id << std::endl;
                std::cout << "  State: " << st.state;
                std::cout << " (updated " << age << "s ago)" << std::endl;
                std::cout << "  Heartbeat: " << st.heartbeat_counter << std::endl;
                
                if (!st.error_message.empty()) {
                    std::cout << "  ERROR: " << st.error_message << std::endl;
                }
                
                std::cout << "  Metrics:" << std::endl;
                for (const auto& [metric, value] : st.metrics) {
                    std::cout << "    " << std::setw(25) << std::left << metric 
                              << ": " << std::fixed << std::setprecision(2) << value << std::endl;
                }
                std::cout << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}