// Example: Data + Command communication in one process using new byte-based interface
// This demonstrates the refactored ZMQTransport with separated serialization

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "ZMQTransport.hpp"
#include "DataProcessor.hpp"
#include "delila/core/EventData.hpp"
#include "delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;
using namespace std::chrono_literals;

// Helper function to create test EventData
std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEvents(size_t count) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    for (size_t i = 0; i < count; ++i) {
        auto event = std::make_unique<EventData>();
        event->timeStampNs = 1000000.0 + i;
        event->energy = 1000 + (i * 10);
        event->energyShort = 500 + (i * 5);
        events->push_back(std::move(event));
    }
    
    return events;
}

// Simple DAQ Source (Publisher)
class DAQSource {
public:
    DAQSource() : processor_(), sequence_number_(0), running_(false) {
        // Configure as publisher for data, and command client
        TransportConfig config;
        config.data_address = "tcp://127.0.0.1:6000";        // Data channel
        config.command_address = "tcp://127.0.0.1:6001";     // Command channel  
        config.status_address = "tcp://127.0.0.1:6002";      // Status channel
        
        config.bind_data = true;      // Bind data port (we're publisher)
        config.bind_command = false;  // Connect to command server
        config.bind_status = false;   // Connect to status server
        
        config.data_pattern = "PUB";
        config.is_publisher = true;
        config.server_mode = false;   // We're not a command/status server
        
        if (!transport_.Configure(config)) {
            throw std::runtime_error("Failed to configure DAQ source");
        }
        
        if (!transport_.Connect()) {
            throw std::runtime_error("Failed to connect DAQ source");
        }
        
        std::cout << "DAQ Source: Connected\n";
        std::cout << "  - Data publisher: tcp://127.0.0.1:6000\n";
        std::cout << "  - Command client: tcp://127.0.0.1:6001\n";
        std::cout << "  - Status client: tcp://127.0.0.1:6002\n";
    }
    
    void Start() {
        running_ = true;
        data_thread_ = std::thread(&DAQSource::DataLoop, this);
        command_thread_ = std::thread(&DAQSource::CommandLoop, this);
        std::cout << "DAQ Source: Started data and command threads\n";
    }
    
    void Stop() {
        running_ = false;
        if (data_thread_.joinable()) data_thread_.join();
        if (command_thread_.joinable()) command_thread_.join();
        std::cout << "DAQ Source: Stopped\n";
    }
    
private:
    void DataLoop() {
        std::cout << "DAQ Source: Data loop started\n";
        
        while (running_) {
            // Create some events
            auto events = CreateTestEvents(10);
            
            // NEW: Serialize using external DataProcessor
            auto serialized_data = processor_.Process(events, ++sequence_number_);
            
            if (serialized_data) {
                // NEW: Send raw bytes via transport
                bool sent = transport_.SendBytes(serialized_data);
                
                if (sent) {
                    std::cout << "DAQ Source: Sent " << events->size() 
                              << " events (seq: " << sequence_number_ << ")\n";
                } else {
                    std::cout << "DAQ Source: Failed to send data\n";
                }
            }
            
            // Send status update
            ComponentStatus status;
            status.component_id = "daq_source";
            status.state = "running";
            status.timestamp = std::chrono::system_clock::now();
            status.metrics["events_sent"] = static_cast<double>(sequence_number_);
            
            transport_.SendStatus(status);
            
            std::this_thread::sleep_for(1s);
        }
    }
    
    void CommandLoop() {
        std::cout << "DAQ Source: Command loop started\n";
        
        while (running_) {
            // Check for commands (this would typically be event-driven)
            std::this_thread::sleep_for(500ms);
            
            // In a real implementation, you might:
            // auto command = transport_.ReceiveCommand();
            // if (command) { processCommand(*command); }
        }
    }
    
    ZMQTransport transport_;
    DataProcessor processor_;
    uint64_t sequence_number_;
    std::atomic<bool> running_;
    std::thread data_thread_;
    std::thread command_thread_;
};

// DAQ Monitor (Subscriber + Command/Status Server)
class DAQMonitor {
public:
    DAQMonitor() : processor_(), received_count_(0), running_(false) {
        // Configure as subscriber for data, and command/status server
        TransportConfig config;
        config.data_address = "tcp://127.0.0.1:6000";        // Data channel
        config.command_address = "tcp://127.0.0.1:6001";     // Command channel
        config.status_address = "tcp://127.0.0.1:6002";      // Status channel
        
        config.bind_data = false;     // Connect to data publisher
        config.bind_command = true;   // Bind command port (we're server)
        config.bind_status = true;    // Bind status port (we're server)
        
        config.data_pattern = "SUB";
        config.is_publisher = false;
        config.server_mode = true;    // We serve commands and status requests
        config.command_server = true;
        config.status_server = true;
        
        if (!transport_.Configure(config)) {
            throw std::runtime_error("Failed to configure DAQ monitor");
        }
        
        if (!transport_.Connect()) {
            throw std::runtime_error("Failed to connect DAQ monitor");
        }
        
        std::cout << "DAQ Monitor: Connected\n";
        std::cout << "  - Data subscriber: tcp://127.0.0.1:6000\n";
        std::cout << "  - Command server: tcp://127.0.0.1:6001\n";
        std::cout << "  - Status server: tcp://127.0.0.1:6002\n";
    }
    
    void Start() {
        running_ = true;
        data_thread_ = std::thread(&DAQMonitor::DataLoop, this);
        command_thread_ = std::thread(&DAQMonitor::CommandLoop, this);
        status_thread_ = std::thread(&DAQMonitor::StatusLoop, this);
        std::cout << "DAQ Monitor: Started data, command, and status threads\n";
    }
    
    void Stop() {
        running_ = false;
        if (data_thread_.joinable()) data_thread_.join();
        if (command_thread_.joinable()) command_thread_.join();
        if (status_thread_.joinable()) status_thread_.join();
        std::cout << "DAQ Monitor: Stopped\n";
    }
    
    uint64_t GetReceivedCount() const { return received_count_; }
    
private:
    void DataLoop() {
        std::cout << "DAQ Monitor: Data loop started\n";
        
        while (running_) {
            // NEW: Receive raw bytes
            auto received_bytes = transport_.ReceiveBytes();
            
            if (received_bytes) {
                // NEW: Deserialize using external DataProcessor
                auto [events, sequence_number] = processor_.Decode(received_bytes);
                
                if (events && !events->empty()) {
                    received_count_++;
                    std::cout << "DAQ Monitor: Received " << events->size() 
                              << " events (seq: " << sequence_number 
                              << ", total packets: " << received_count_ << ")\n";
                    
                    // Process events...
                    for (const auto& event : *events) {
                        // std::cout << "  Event: ch=" << event->channel 
                        //           << " energy=" << event->energy << "\n";
                    }
                }
            }
            
            std::this_thread::sleep_for(10ms); // Don't busy wait
        }
    }
    
    void CommandLoop() {
        std::cout << "DAQ Monitor: Command server started\n";
        
        // Start the server
        if (!transport_.StartServer()) {
            std::cout << "DAQ Monitor: Failed to start command server\n";
            return;
        }
        
        while (running_) {
            // Wait for command requests
            auto command = transport_.WaitForCommand();
            if (command) {
                std::cout << "DAQ Monitor: Received command: " << static_cast<int>(command->type) 
                          << " for " << command->target_component << "\n";
                
                // Process command and send response
                CommandResponse response;
                response.command_id = command->command_id;
                response.success = true;
                response.message = "Command processed successfully";
                response.timestamp = std::chrono::system_clock::now();
                
                transport_.SendCommandResponse(response);
            }
        }
    }
    
    void StatusLoop() {
        std::cout << "DAQ Monitor: Status server started\n";
        
        while (running_) {
            // Wait for status requests
            auto status_request = transport_.WaitForStatusRequest();
            if (status_request) {
                std::cout << "DAQ Monitor: Status request received\n";
                
                // Send our status
                ComponentStatus status;
                status.component_id = "daq_monitor";
                status.state = "running";
                status.timestamp = std::chrono::system_clock::now();
                status.metrics["packets_received"] = static_cast<double>(received_count_);
                
                transport_.SendStatusResponse(status);
            }
        }
    }
    
    ZMQTransport transport_;
    DataProcessor processor_;
    std::atomic<uint64_t> received_count_;
    std::atomic<bool> running_;
    std::thread data_thread_;
    std::thread command_thread_;
    std::thread status_thread_;
};

int main() {
    std::cout << "=== Data + Command Communication Example ===\n";
    std::cout << "This demonstrates:\n";
    std::cout << "1. NEW byte-based transport (SendBytes/ReceiveBytes)\n";
    std::cout << "2. External serialization (user controls serialization)\n";
    std::cout << "3. Data + Command + Status in one process\n";
    std::cout << "4. Clean separation of transport and serialization\n\n";
    
    try {
        // Small delay to ensure clean startup
        std::this_thread::sleep_for(100ms);
        
        // Create and start monitor first (it binds the server ports)
        std::cout << "Creating DAQ Monitor...\n";
        DAQMonitor monitor;
        monitor.Start();
        
        // Give monitor time to start servers
        std::this_thread::sleep_for(500ms);
        
        // Create and start source
        std::cout << "\nCreating DAQ Source...\n";
        DAQSource source;
        source.Start();
        
        std::cout << "\n=== Running for 10 seconds ===\n";
        
        // Let them run for a while
        std::this_thread::sleep_for(10s);
        
        std::cout << "\n=== Stopping ===\n";
        source.Stop();
        monitor.Stop();
        
        std::cout << "\nFinal Results:\n";
        std::cout << "  Packets received: " << monitor.GetReceivedCount() << "\n";
        std::cout << "  SUCCESS: Data and commands worked in one process!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}