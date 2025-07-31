#include <iostream>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    std::cout << "Testing command server setup..." << std::endl;
    
    ZMQTransport transport;
    TransportConfig config;
    config.command_address = "tcp://*:5557";
    config.server_mode = true;
    config.command_server = true;
    config.bind_command = true;
    
    // Disable data channel for command-only server
    config.data_pattern = "PAIR";  // Use PAIR pattern to avoid conflicts
    config.data_address = config.command_address;
    config.status_address = config.command_address;
    config.bind_data = false;
    
    std::cout << "Configuring..." << std::endl;
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    std::cout << "Connecting..." << std::endl;
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "Starting server..." << std::endl;
    if (!transport.StartServer()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "Server started successfully!" << std::endl;
    
    return 0;
}