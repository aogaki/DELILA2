#include <iostream>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

void test_reporter() {
    std::cout << "Testing status reporter setup..." << std::endl;
    
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
    
    std::cout << "Status address: " << config.status_address << std::endl;
    std::cout << "Data address: " << config.data_address << std::endl;
    std::cout << "Addresses are different: " << (config.status_address != config.data_address) << std::endl;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return;
    }
    
    std::cout << "Connected successfully!" << std::endl;
    
    // Try to send status
    ComponentStatus status;
    status.component_id = "test";
    status.state = "RUNNING";
    
    if (transport.SendStatus(status)) {
        std::cout << "Status sent successfully!" << std::endl;
    } else {
        std::cout << "Failed to send status" << std::endl;
    }
}

int main() {
    test_reporter();
    return 0;
}