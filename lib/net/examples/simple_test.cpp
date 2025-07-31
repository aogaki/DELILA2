#include <iostream>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

void test_publisher() {
    std::cout << "Testing publisher..." << std::endl;
    
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5555";
    config.data_pattern = "PUB";
    config.bind_data = true;
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    std::cout << "Configuring..." << std::endl;
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return;
    }
    
    std::cout << "Connecting..." << std::endl;
    try {
        if (!transport.Connect()) {
            std::cerr << "Failed to connect" << std::endl;
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during connect: " << e.what() << std::endl;
        return;
    }
    
    std::cout << "Connected successfully!" << std::endl;
    
    // Send a test event
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->energy = 1234;
    events->push_back(std::move(event));
    
    if (transport.Send(events)) {
        std::cout << "Event sent successfully!" << std::endl;
    } else {
        std::cout << "Failed to send event" << std::endl;
    }
}

void test_subscriber() {
    std::cout << "Testing subscriber..." << std::endl;
    
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5555";
    config.data_pattern = "SUB";
    config.bind_data = false;
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    std::cout << "Configuring..." << std::endl;
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return;
    }
    
    std::cout << "Connecting..." << std::endl;
    try {
        if (!transport.Connect()) {
            std::cerr << "Failed to connect" << std::endl;
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during connect: " << e.what() << std::endl;
        return;
    }
    
    std::cout << "Connected successfully!" << std::endl;
    
    // Try to receive
    auto [events, seq] = transport.Receive();
    if (events) {
        std::cout << "Received " << events->size() << " events" << std::endl;
    } else {
        std::cout << "No events received (timeout is normal)" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " pub|sub" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    if (mode == "pub") {
        test_publisher();
    } else if (mode == "sub") {
        test_subscriber();
    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        return 1;
    }
    
    return 0;
}