#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

// Publisher function
void publisher() {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5560";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    // Disable status and command channels
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "Publisher: Failed to configure" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Publisher: Failed to connect" << std::endl;
        return;
    }
    
    std::cout << "Publisher: Started on port 5560" << std::endl;
    
    // Give subscribers time to connect
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Send 10 batches of events
    for (int batch = 0; batch < 10; ++batch) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        // Create a single event per batch for clarity
        auto event = std::make_unique<EventData>();
        event->timeStampNs = std::chrono::system_clock::now().time_since_epoch().count();
        event->energy = 1000 + batch * 100;
        event->module = 0;
        event->channel = batch;
        
        events->push_back(std::move(event));
        
        if (transport.Send(events)) {
            std::cout << "Publisher: Sent batch " << batch 
                      << " with energy " << (1000 + batch * 100) << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "Publisher: Finished sending" << std::endl;
}

// Subscriber function
void subscriber(int id, const std::string& name) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5560";
    config.data_pattern = "SUB";
    config.bind_data = false;
    
    // Disable status and command channels
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << name << ": Failed to configure" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << name << ": Failed to connect" << std::endl;
        return;
    }
    
    std::cout << name << ": Connected to publisher" << std::endl;
    
    // Receive events for 7 seconds
    auto start_time = std::chrono::steady_clock::now();
    int received_count = 0;
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(7)) {
        auto [events, sequence] = transport.Receive();
        
        if (events && !events->empty()) {
            const auto& event = (*events)[0];
            std::cout << name << ": Received event with energy " 
                      << event->energy << " (sequence " << sequence << ")" << std::endl;
            received_count++;
        }
    }
    
    std::cout << name << ": Total received: " << received_count << " events" << std::endl;
}

int main() {
    std::cout << "=== One-to-Many Broadcasting Demo ===" << std::endl;
    std::cout << "Starting 1 publisher and 3 subscribers..." << std::endl;
    std::cout << std::endl;
    
    // Start publisher thread
    std::thread pub_thread(publisher);
    
    // Give publisher time to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Start multiple subscriber threads
    std::vector<std::thread> sub_threads;
    sub_threads.emplace_back(subscriber, 1, "Subscriber-1");
    sub_threads.emplace_back(subscriber, 2, "Subscriber-2");
    sub_threads.emplace_back(subscriber, 3, "Subscriber-3");
    
    // Wait for all threads to complete
    pub_thread.join();
    for (auto& t : sub_threads) {
        t.join();
    }
    
    std::cout << std::endl;
    std::cout << "Demo completed. All subscribers received the same data." << std::endl;
    
    return 0;
}