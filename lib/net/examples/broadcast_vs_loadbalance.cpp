#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

std::atomic<bool> running{true};

// PUB/SUB Demo - All subscribers get all messages
void pubsub_demo() {
    std::cout << "\n=== PUB/SUB Pattern (Broadcasting) ===" << std::endl;
    std::cout << "All subscribers receive ALL messages\n" << std::endl;
    
    // Publisher
    std::thread pub_thread([]() {
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://*:5561";
        config.data_pattern = "PUB";
        config.bind_data = true;
        config.status_address = config.data_address;
        config.command_address = config.data_address;
        
        transport.Configure(config);
        transport.Connect();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        for (int i = 0; i < 6; ++i) {
            auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
            auto event = std::make_unique<EventData>();
            event->energy = i;
            events->push_back(std::move(event));
            
            transport.Send(events);
            std::cout << "PUB: Sent message " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
    
    // Subscribers
    auto subscriber = [](int id) {
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://localhost:5561";
        config.data_pattern = "SUB";
        config.bind_data = false;
        config.status_address = config.data_address;
        config.command_address = config.data_address;
        
        transport.Configure(config);
        transport.Connect();
        
        int count = 0;
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            auto [events, seq] = transport.Receive();
            if (events && !events->empty()) {
                std::cout << "  SUB-" << id << ": Received message " 
                          << (*events)[0]->energy << std::endl;
                count++;
            }
        }
        
        std::cout << "  SUB-" << id << ": Total received: " << count << " messages" << std::endl;
    };
    
    std::thread sub1(subscriber, 1);
    std::thread sub2(subscriber, 2);
    
    pub_thread.join();
    sub1.join();
    sub2.join();
}

// PUSH/PULL Demo - Messages are distributed among workers
void pushpull_demo() {
    std::cout << "\n=== PUSH/PULL Pattern (Load Balancing) ===" << std::endl;
    std::cout << "Messages are distributed among workers\n" << std::endl;
    
    // Pusher
    std::thread push_thread([]() {
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://*:5562";
        config.data_pattern = "PUSH";
        config.bind_data = true;
        config.status_address = config.data_address;
        config.command_address = config.data_address;
        
        transport.Configure(config);
        transport.Connect();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        for (int i = 0; i < 6; ++i) {
            auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
            auto event = std::make_unique<EventData>();
            event->energy = i;
            events->push_back(std::move(event));
            
            transport.Send(events);
            std::cout << "PUSH: Sent message " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
    
    // Pullers
    auto puller = [](int id) {
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://localhost:5562";
        config.data_pattern = "PULL";
        config.bind_data = false;
        config.status_address = config.data_address;
        config.command_address = config.data_address;
        
        transport.Configure(config);
        transport.Connect();
        
        int count = 0;
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            auto [events, seq] = transport.Receive();
            if (events && !events->empty()) {
                std::cout << "  PULL-" << id << ": Received message " 
                          << (*events)[0]->energy << std::endl;
                count++;
            }
        }
        
        std::cout << "  PULL-" << id << ": Total received: " << count << " messages" << std::endl;
    };
    
    std::thread pull1(puller, 1);
    std::thread pull2(puller, 2);
    
    push_thread.join();
    pull1.join();
    pull2.join();
}

int main() {
    std::cout << "=== Broadcasting vs Load Balancing Demo ===" << std::endl;
    std::cout << "This demo shows the difference between PUB/SUB and PUSH/PULL patterns" << std::endl;
    
    // Run PUB/SUB demo
    pubsub_demo();
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Run PUSH/PULL demo
    pushpull_demo();
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "- PUB/SUB: Each subscriber gets ALL messages (broadcasting)" << std::endl;
    std::cout << "- PUSH/PULL: Messages are distributed among workers (load balancing)" << std::endl;
    
    return 0;
}