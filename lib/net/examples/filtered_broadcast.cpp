#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

// Publisher that sends events from different modules
void multi_module_publisher() {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5563";
    config.data_pattern = "PUB";
    config.bind_data = true;
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
    
    std::cout << "Publisher: Broadcasting events from 3 different modules" << std::endl;
    
    // Give subscribers time to connect
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Send events from different modules
    for (int i = 0; i < 9; ++i) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        auto event = std::make_unique<EventData>();
        
        event->module = i % 3;  // Module 0, 1, or 2
        event->channel = i;
        event->energy = 1000 + i * 100;
        event->timeStampNs = std::chrono::system_clock::now().time_since_epoch().count();
        
        events->push_back(std::move(event));
        
        if (transport.Send(events)) {
            std::cout << "Publisher: Sent event " << i 
                      << " from module " << (int)event->module 
                      << " with energy " << event->energy << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    std::cout << "Publisher: Finished broadcasting" << std::endl;
}

// Subscriber that processes all events
void all_events_subscriber() {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5563";
    config.data_pattern = "SUB";
    config.bind_data = false;
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "AllEventsSubscriber: Failed to configure" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "AllEventsSubscriber: Failed to connect" << std::endl;
        return;
    }
    
    std::cout << "AllEventsSubscriber: Receiving ALL events" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int total_events = 0;
    std::vector<int> module_counts(3, 0);
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(4)) {
        auto [events, sequence] = transport.Receive();
        
        if (events && !events->empty()) {
            const auto& event = (*events)[0];
            module_counts[event->module]++;
            total_events++;
            
            std::cout << "  AllEvents: Received event from module " 
                      << (int)event->module << " with energy " << event->energy << std::endl;
        }
    }
    
    std::cout << "AllEventsSubscriber Summary:" << std::endl;
    std::cout << "  Total events: " << total_events << std::endl;
    for (int i = 0; i < 3; ++i) {
        std::cout << "  Module " << i << ": " << module_counts[i] << " events" << std::endl;
    }
}

// Subscriber that only processes high-energy events
void high_energy_subscriber() {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5563";
    config.data_pattern = "SUB";
    config.bind_data = false;
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "HighEnergySubscriber: Failed to configure" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "HighEnergySubscriber: Failed to connect" << std::endl;
        return;
    }
    
    std::cout << "HighEnergySubscriber: Filtering for high-energy events (>1500)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int total_events = 0;
    int high_energy_events = 0;
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(4)) {
        auto [events, sequence] = transport.Receive();
        
        if (events && !events->empty()) {
            const auto& event = (*events)[0];
            total_events++;
            
            // Application-level filtering
            if (event->energy > 1500) {
                high_energy_events++;
                std::cout << "  HighEnergy: Accepted event from module " 
                          << (int)event->module << " with energy " << event->energy << std::endl;
            }
        }
    }
    
    std::cout << "HighEnergySubscriber Summary:" << std::endl;
    std::cout << "  Total received: " << total_events << " events" << std::endl;
    std::cout << "  High-energy events: " << high_energy_events << " events" << std::endl;
    std::cout << "  Filtered out: " << (total_events - high_energy_events) << " events" << std::endl;
}

// Subscriber that only processes events from specific module
void module_specific_subscriber(uint8_t target_module) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5563";
    config.data_pattern = "SUB";
    config.bind_data = false;
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "ModuleSubscriber-" << (int)target_module << ": Failed to configure" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "ModuleSubscriber-" << (int)target_module << ": Failed to connect" << std::endl;
        return;
    }
    
    std::cout << "ModuleSubscriber-" << (int)target_module 
              << ": Filtering for module " << (int)target_module << " only" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int total_events = 0;
    int module_events = 0;
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(4)) {
        auto [events, sequence] = transport.Receive();
        
        if (events && !events->empty()) {
            const auto& event = (*events)[0];
            total_events++;
            
            // Application-level filtering
            if (event->module == target_module) {
                module_events++;
                std::cout << "  Module-" << (int)target_module 
                          << ": Accepted event with energy " << event->energy << std::endl;
            }
        }
    }
    
    std::cout << "ModuleSubscriber-" << (int)target_module << " Summary:" << std::endl;
    std::cout << "  Total received: " << total_events << " events" << std::endl;
    std::cout << "  Module " << (int)target_module << " events: " << module_events << " events" << std::endl;
    std::cout << "  Filtered out: " << (total_events - module_events) << " events" << std::endl;
}

int main() {
    std::cout << "=== Filtered Broadcasting Demo ===" << std::endl;
    std::cout << "Shows different subscribers filtering the same broadcast stream" << std::endl;
    std::cout << std::endl;
    
    // Start publisher
    std::thread pub_thread(multi_module_publisher);
    
    // Give publisher time to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Start different types of subscribers
    std::thread all_sub(all_events_subscriber);
    std::thread high_energy_sub(high_energy_subscriber);
    std::thread module0_sub(module_specific_subscriber, 0);
    std::thread module1_sub(module_specific_subscriber, 1);
    
    // Wait for all threads
    pub_thread.join();
    all_sub.join();
    high_energy_sub.join();
    module0_sub.join();
    module1_sub.join();
    
    std::cout << std::endl;
    std::cout << "=== Key Points ===" << std::endl;
    std::cout << "1. All subscribers receive ALL events (PUB/SUB broadcasting)" << std::endl;
    std::cout << "2. Each subscriber can implement its own filtering logic" << std::endl;
    std::cout << "3. This allows flexible, decoupled data distribution" << std::endl;
    std::cout << "4. Note: ZeroMQ SUB sockets support prefix-based filtering at the socket level" << std::endl;
    std::cout << "   (not demonstrated here - requires message envelope design)" << std::endl;
    
    return 0;
}