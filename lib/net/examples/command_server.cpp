#include <iostream>
#include <map>
#include <functional>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

class CommandServer {
private:
    ZMQTransport transport;
    std::map<CommandType, std::function<CommandResponse(const Command&)>> handlers;
    bool running = false;
    
public:
    CommandServer() {
        // Register command handlers
        handlers[CommandType::START] = [this](const Command& cmd) {
            return handleStart(cmd);
        };
        
        handlers[CommandType::STOP] = [this](const Command& cmd) {
            return handleStop(cmd);
        };
        
        handlers[CommandType::CONFIGURE] = [this](const Command& cmd) {
            return handleConfigure(cmd);
        };
    }
    
    bool Initialize(const std::string& address = "tcp://*:5557") {
        TransportConfig config;
        config.command_address = address;
        config.server_mode = true;
        config.command_server = true;
        config.bind_command = true;  // Bind the command socket as a server
        
        // Set data and status addresses to be different but not used
        config.data_address = "tcp://localhost:9998";  
        config.status_address = "tcp://localhost:9999";
        config.data_pattern = "SUB";  // As SUB with bind=false, won't conflict
        config.bind_data = false;
        config.bind_status = false;
        
        if (!transport.Configure(config)) {
            return false;
        }
        
        if (!transport.Connect()) {
            return false;
        }
        
        return transport.StartServer();
    }
    
    void Run() {
        std::cout << "Command server running..." << std::endl;
        running = true;
        
        while (running) {
            auto command = transport.WaitForCommand(1000);
            
            if (command) {
                std::cout << "Received command: " << static_cast<int>(command->type) 
                          << " for " << command->target_component << std::endl;
                
                CommandResponse response;
                response.command_id = command->command_id;
                response.timestamp = std::chrono::system_clock::now();
                
                // Process command
                if (handlers.find(command->type) != handlers.end()) {
                    response = handlers[command->type](*command);
                } else {
                    response.success = false;
                    response.message = "Unknown command type";
                }
                
                transport.SendCommandResponse(response);
            }
        }
    }
    
private:
    CommandResponse handleStart(const Command& cmd) {
        CommandResponse response;
        response.command_id = cmd.command_id;
        response.success = true;
        response.message = "System started successfully";
        response.timestamp = std::chrono::system_clock::now();
        return response;
    }
    
    CommandResponse handleStop(const Command& cmd) {
        CommandResponse response;
        response.command_id = cmd.command_id;
        response.success = true;
        response.message = "System stopped";
        response.timestamp = std::chrono::system_clock::now();
        running = false;
        return response;
    }
    
    CommandResponse handleConfigure(const Command& cmd) {
        CommandResponse response;
        response.command_id = cmd.command_id;
        response.success = true;
        response.message = "Configuration applied";
        response.timestamp = std::chrono::system_clock::now();
        
        // Echo back configuration parameters
        response.result_data = cmd.parameters;
        
        return response;
    }
};

int main() {
    CommandServer server;
    
    if (!server.Initialize()) {
        std::cerr << "Failed to initialize command server" << std::endl;
        return 1;
    }
    
    server.Run();
    return 0;
}