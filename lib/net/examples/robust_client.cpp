#include <iostream>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

class RobustDataClient {
private:
    ZMQTransport transport;
    TransportConfig config;
    bool connected = false;
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 5;
    
public:
    RobustDataClient(const std::string& server_address) {
        config.data_address = server_address;
        config.data_pattern = "SUB";
        config.bind_data = false;
        
        // Disable status and command channels
        config.status_address = config.data_address;
        config.command_address = config.data_address;
    }
    
    bool Connect() {
        if (!transport.Configure(config)) {
            std::cerr << "Failed to configure transport" << std::endl;
            return false;
        }
        
        connected = transport.Connect();
        if (connected) {
            std::cout << "Connected to " << config.data_address << std::endl;
            reconnect_attempts = 0;
        }
        
        return connected;
    }
    
    void Run() {
        while (true) {
            if (!connected) {
                if (!Reconnect()) {
                    std::cerr << "Failed to reconnect after " << max_reconnect_attempts 
                              << " attempts. Exiting." << std::endl;
                    break;
                }
            }
            
            try {
                auto [events, sequence] = transport.Receive();
                
                if (events) {
                    ProcessEvents(events, sequence);
                } else {
                    // No data received, check connection
                    if (!transport.IsConnected()) {
                        std::cerr << "Connection lost" << std::endl;
                        connected = false;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error receiving data: " << e.what() << std::endl;
                connected = false;
            }
        }
    }
    
private:
    bool Reconnect() {
        while (reconnect_attempts < max_reconnect_attempts) {
            reconnect_attempts++;
            std::cout << "Reconnection attempt " << reconnect_attempts 
                      << "/" << max_reconnect_attempts << std::endl;
            
            // Disconnect and wait
            transport.Disconnect();
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_attempts));
            
            // Try to reconnect
            if (Connect()) {
                return true;
            }
        }
        
        return false;
    }
    
    void ProcessEvents(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events,
                       uint64_t sequence) {
        std::cout << "Received " << events->size() << " events, sequence: " << sequence << std::endl;
        
        // Process events here
        for (const auto& event : *events) {
            if (event->HasPileup()) {
                std::cout << "Warning: Event has pileup flag set" << std::endl;
            }
            if (event->HasOverRange()) {
                std::cout << "Warning: Event has over-range flag set" << std::endl;
            }
        }
    }
};

int main() {
    RobustDataClient client("tcp://localhost:5555");
    
    if (!client.Connect()) {
        std::cerr << "Initial connection failed" << std::endl;
        return 1;
    }
    
    client.Run();
    
    return 0;
}