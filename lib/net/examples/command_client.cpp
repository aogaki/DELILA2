#include <iostream>
#include <chrono>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    // Configure client
    ZMQTransport transport;
    TransportConfig config;
    config.command_address = "tcp://localhost:5557";
    config.server_mode = false;
    config.bind_command = false;
    
    // Set data and status addresses to be different but not used
    config.data_address = "tcp://localhost:9996";  
    config.status_address = "tcp://localhost:9997";
    config.data_pattern = "SUB";
    config.bind_data = false;
    config.bind_status = false;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Send START command
    Command start_cmd;
    start_cmd.command_id = "cmd_001";
    start_cmd.type = CommandType::START;
    start_cmd.target_component = "digitizer_01";
    start_cmd.timestamp = std::chrono::system_clock::now();
    
    std::cout << "Sending START command..." << std::endl;
    auto response = transport.SendCommand(start_cmd);
    
    if (response.success) {
        std::cout << "Response: " << response.message << std::endl;
    } else {
        std::cerr << "Command failed: " << response.message << std::endl;
    }
    
    // Send CONFIGURE command
    Command config_cmd;
    config_cmd.command_id = "cmd_002";
    config_cmd.type = CommandType::CONFIGURE;
    config_cmd.target_component = "digitizer_01";
    config_cmd.parameters["sample_rate"] = "1000000";
    config_cmd.parameters["channel_mask"] = "0xFF";
    config_cmd.timestamp = std::chrono::system_clock::now();
    
    std::cout << "Sending CONFIGURE command..." << std::endl;
    response = transport.SendCommand(config_cmd);
    
    if (response.success) {
        std::cout << "Response: " << response.message << std::endl;
        for (const auto& [key, value] : response.result_data) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
    
    // Send STOP command
    Command stop_cmd;
    stop_cmd.command_id = "cmd_003";
    stop_cmd.type = CommandType::STOP;
    stop_cmd.target_component = "digitizer_01";
    stop_cmd.timestamp = std::chrono::system_clock::now();
    
    std::cout << "Sending STOP command..." << std::endl;
    response = transport.SendCommand(stop_cmd);
    std::cout << "Response: " << response.message << std::endl;
    
    return 0;
}