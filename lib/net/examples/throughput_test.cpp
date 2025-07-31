#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

std::atomic<uint64_t> total_bytes{0};
std::atomic<uint64_t> total_events{0};

void publisher_thread(int batch_size, int event_size) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5558";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    // Disable status and command channels
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure publisher" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect publisher" << std::endl;
        return;
    }
    
    // Enable optimizations
    transport.EnableZeroCopy(true);
    transport.EnableMemoryPool(true);
    transport.SetMemoryPoolSize(50);
    
    while (true) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int i = 0; i < batch_size; ++i) {
            auto event = std::make_unique<EventData>(event_size);
            event->timeStampNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            event->energy = i;
            events->push_back(std::move(event));
        }
        
        if (transport.Send(events)) {
            total_events += batch_size;
            total_bytes += batch_size * (EVENTDATA_SIZE + event_size * sizeof(int32_t) * 2);
        }
    }
}

void subscriber_thread() {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5558";
    config.data_pattern = "SUB";
    config.bind_data = false;
    
    // Disable status and command channels
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure subscriber" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect subscriber" << std::endl;
        return;
    }
    
    while (true) {
        auto [events, sequence] = transport.Receive();
        // Just receive and discard
    }
}

int main() {
    const int batch_size = 100;
    const int event_size = 1024;
    
    std::cout << "Starting throughput test..." << std::endl;
    std::cout << "Batch size: " << batch_size << " events" << std::endl;
    std::cout << "Event size: " << event_size << " samples" << std::endl;
    
    // Start threads
    std::thread pub_thread(publisher_thread, batch_size, event_size);
    std::thread sub_thread(subscriber_thread);
    
    // Monitor performance
    auto start_time = std::chrono::steady_clock::now();
    uint64_t last_events = 0;
    uint64_t last_bytes = 0;
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        uint64_t current_events = total_events.load();
        uint64_t current_bytes = total_bytes.load();
        
        uint64_t events_diff = current_events - last_events;
        uint64_t bytes_diff = current_bytes - last_bytes;
        
        double mbps = (bytes_diff / 1024.0 / 1024.0);
        
        std::cout << "Events/sec: " << events_diff 
                  << ", MB/sec: " << std::fixed << std::setprecision(2) << mbps
                  << ", Total events: " << current_events << std::endl;
        
        last_events = current_events;
        last_bytes = current_bytes;
    }
    
    pub_thread.join();
    sub_thread.join();
    
    return 0;
}