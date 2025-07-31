#include <iostream>
#include <chrono>
#include <vector>
#include "ZMQTransport.hpp"
#include "Serializer.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

void benchmark_with_pool(bool use_pool, int iterations) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5559";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    // Disable status and command channels
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return;
    }
    
    // Configure memory pool
    transport.EnableMemoryPool(use_pool);
    if (use_pool) {
        transport.SetMemoryPoolSize(20);
    }
    
    // Also configure serializer buffer pool
    Serializer serializer;
    serializer.EnableBufferPool(use_pool);
    if (use_pool) {
        serializer.SetBufferPoolSize(20);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int j = 0; j < 100; ++j) {
            auto event = std::make_unique<EventData>(1024);
            event->energy = i * 100 + j;
            events->push_back(std::move(event));
        }
        
        transport.Send(events);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << (use_pool ? "With pool: " : "Without pool: ")
              << duration.count() << " ms for " << iterations << " iterations" << std::endl;
    
    if (use_pool) {
        std::cout << "Pooled containers: " << transport.GetPooledContainerCount() << std::endl;
    }
}

int main() {
    std::cout << "Memory Pool Performance Comparison" << std::endl;
    std::cout << "==================================" << std::endl;
    
    const int iterations = 1000;
    
    // Warm up
    benchmark_with_pool(false, 10);
    
    // Benchmark without pool
    benchmark_with_pool(false, iterations);
    
    // Benchmark with pool
    benchmark_with_pool(true, iterations);
    
    return 0;
}