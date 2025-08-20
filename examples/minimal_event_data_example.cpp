/**
 * MinimalEventData Example
 * 
 * This example demonstrates the usage of MinimalEventData for efficient
 * data acquisition and transport in DELILA2.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include "delila/core/MinimalEventData.hpp"
#include "delila/net/ZMQTransport.hpp"
#include "delila/net/Serializer.hpp"

using namespace DELILA::Digitizer;
using namespace DELILA::Net;

// Helper function to create test events
std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> 
CreateTestEvents(size_t count, uint8_t module_id) {
    auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    events->reserve(count);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < count; ++i) {
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - start_time).count();
        
        events->push_back(std::make_unique<MinimalEventData>(
            module_id,                    // module
            static_cast<uint8_t>(i % 64), // channel (0-63)
            static_cast<double>(timestamp_ns),
            static_cast<uint16_t>(1000 + (i % 4096)),  // energy
            static_cast<uint16_t>(500 + (i % 2048)),   // energyShort
            i % 4 == 0 ? MinimalEventData::FLAG_PILEUP : 0  // flags
        ));
    }
    
    return events;
}

// Example 1: Basic MinimalEventData creation and inspection
void BasicUsageExample() {
    std::cout << "\n=== Basic MinimalEventData Usage ===\n";
    
    // Create a single event
    MinimalEventData event(
        1,          // module
        5,          // channel
        12345.67,   // timestamp in ns
        1024,       // energy
        512,        // energyShort
        MinimalEventData::FLAG_PILEUP  // flags
    );
    
    // Display event information
    std::cout << "Event Details:\n";
    std::cout << "  Module: " << static_cast<int>(event.module) << "\n";
    std::cout << "  Channel: " << static_cast<int>(event.channel) << "\n";
    std::cout << "  Timestamp: " << event.timeStampNs << " ns\n";
    std::cout << "  Energy: " << event.energy << "\n";
    std::cout << "  Energy Short: " << event.energyShort << "\n";
    std::cout << "  Flags: 0x" << std::hex << event.flags << std::dec << "\n";
    std::cout << "  Size: " << sizeof(MinimalEventData) << " bytes\n";
}

// Example 2: Batch processing and memory efficiency
void BatchProcessingExample() {
    std::cout << "\n=== Batch Processing Example ===\n";
    
    const size_t EVENT_COUNT = 10000;
    
    // Create a batch of events
    auto start = std::chrono::high_resolution_clock::now();
    auto events = CreateTestEvents(EVENT_COUNT, 1);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Created " << EVENT_COUNT << " events in " 
              << duration.count() << " microseconds\n";
    std::cout << "Rate: " << (EVENT_COUNT * 1000000.0 / duration.count()) 
              << " events/second\n";
    
    // Calculate memory usage
    size_t memory_used = sizeof(std::vector<std::unique_ptr<MinimalEventData>>) +
                        EVENT_COUNT * (sizeof(std::unique_ptr<MinimalEventData>) + 
                                     sizeof(MinimalEventData));
    
    std::cout << "Memory used: " << memory_used << " bytes\n";
    std::cout << "Memory per event: " << (memory_used / EVENT_COUNT) << " bytes\n";
    
    // Compare with EventData (estimated)
    size_t eventdata_memory = EVENT_COUNT * 500;  // ~500 bytes per EventData
    std::cout << "EventData would use: " << eventdata_memory << " bytes\n";
    std::cout << "Memory saved: " << (100.0 * (eventdata_memory - memory_used) / eventdata_memory) 
              << "%\n";
}

// Example 3: Serialization and transport
void SerializationExample() {
    std::cout << "\n=== Serialization Example ===\n";
    
    // Create events
    auto events = CreateTestEvents(100, 2);
    
    // Serialize with MinimalEventData format
    Serializer serializer(FORMAT_VERSION_MINIMAL_EVENTDATA);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto encoded = serializer.Encode(events, 12345);  // sequence number
    auto end = std::chrono::high_resolution_clock::now();
    
    if (encoded) {
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        std::cout << "Serialized 100 events:\n";
        std::cout << "  Serialized size: " << encoded->size() << " bytes\n";
        std::cout << "  Time: " << duration.count() << " ns\n";
        std::cout << "  Rate: " << (100 * 1000000000.0 / duration.count()) 
                  << " events/second\n";
        
        // Deserialize
        start = std::chrono::high_resolution_clock::now();
        auto [decoded_events, sequence] = serializer.DecodeMinimalEventData(*encoded);
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        if (decoded_events) {
            std::cout << "Deserialized " << decoded_events->size() << " events:\n";
            std::cout << "  Time: " << duration.count() << " ns\n";
            std::cout << "  Rate: " << (decoded_events->size() * 1000000000.0 / duration.count()) 
                      << " events/second\n";
            std::cout << "  Sequence number: " << sequence << "\n";
        }
    }
}

// Example 4: Network transport simulation (OLD API - Deprecated)
void NetworkTransportExample_OldAPI() {
    std::cout << "\n=== Network Transport Example (OLD API - Deprecated) ===\n";
    
    try {
        // Setup publisher
        ZMQTransport publisher;
        TransportConfig pub_config;
        pub_config.data_address = "tcp://*:15560";
        pub_config.data_pattern = "PUSH";
        pub_config.bind_data = true;
        
        if (!publisher.Configure(pub_config) || !publisher.Connect()) {
            std::cerr << "Failed to setup publisher\n";
            return;
        }
        
        // Setup subscriber
        ZMQTransport subscriber;
        TransportConfig sub_config;
        sub_config.data_address = "tcp://localhost:15560";
        sub_config.data_pattern = "PULL";
        sub_config.bind_data = false;
        
        if (!subscriber.Configure(sub_config) || !subscriber.Connect()) {
            std::cerr << "Failed to setup subscriber\n";
            return;
        }
        
        // Allow connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Send events (OLD API - uses internal serialization)
        auto events = CreateTestEvents(1000, 3);
        std::cout << "Sending 1000 MinimalEventData events (OLD API)...\n";
        std::cout << "WARNING: Using deprecated SendMinimal method\n";
        
        if (publisher.SendMinimal(events)) {
            std::cout << "Events sent successfully\n";
            
            // Check data type
            auto data_type = subscriber.PeekDataType();
            if (data_type == ZMQTransport::DataType::MINIMAL_EVENTDATA) {
                std::cout << "Detected MinimalEventData format\n";
                
                // Receive events (OLD API - uses internal deserialization)
                std::cout << "WARNING: Using deprecated ReceiveMinimal method\n";
                auto [received_events, seq] = subscriber.ReceiveMinimal();
                if (received_events) {
                    std::cout << "Received " << received_events->size() << " events\n";
                    std::cout << "Sequence number: " << seq << "\n";
                    
                    // Verify first event
                    if (!received_events->empty()) {
                        const auto& first = (*received_events)[0];
                        std::cout << "First event - Module: " 
                                  << static_cast<int>(first->module) 
                                  << ", Channel: " 
                                  << static_cast<int>(first->channel) << "\n";
                    }
                }
            }
        }
        
        publisher.Disconnect();
        subscriber.Disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "Transport error: " << e.what() << "\n";
    }
}

// Example 4b: Network transport simulation (NEW API - Recommended)
void NetworkTransportExample_NewAPI() {
    std::cout << "\n=== Network Transport Example (NEW API - Recommended) ===\n";
    
    try {
        // Setup publisher
        ZMQTransport publisher;
        TransportConfig pub_config;
        pub_config.data_address = "tcp://*:15561";  // Different port
        pub_config.data_pattern = "PUSH";
        pub_config.bind_data = true;
        
        if (!publisher.Configure(pub_config) || !publisher.Connect()) {
            std::cerr << "Failed to setup publisher\n";
            return;
        }
        
        // Setup subscriber
        ZMQTransport subscriber;
        TransportConfig sub_config;
        sub_config.data_address = "tcp://localhost:15561";
        sub_config.data_pattern = "PULL";
        sub_config.bind_data = false;
        
        if (!subscriber.Configure(sub_config) || !subscriber.Connect()) {
            std::cerr << "Failed to setup subscriber\n";
            return;
        }
        
        // Allow connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // NEW API: External serialization + byte transport
        auto events = CreateTestEvents(1000, 3);
        std::cout << "Sending 1000 MinimalEventData events (NEW API)...\n";
        
        // Step 1: Serialize externally
        Serializer serializer;
        uint64_t sequence_number = 42;
        auto serialized_bytes = serializer.Encode(events, sequence_number);
        
        if (serialized_bytes) {
            std::cout << "Serialized to " << serialized_bytes->size() << " bytes\n";
            
            // Step 2: Send raw bytes
            if (publisher.SendBytes(serialized_bytes)) {
                std::cout << "Bytes sent successfully\n";
                
                // Step 3: Receive raw bytes
                auto received_bytes = subscriber.ReceiveBytes();
                if (received_bytes) {
                    std::cout << "Received " << received_bytes->size() << " bytes\n";
                    
                    // Step 4: Deserialize externally
                    Serializer deserializer;
                    auto [received_events, received_seq] = deserializer.Decode(received_bytes);
                    
                    if (received_events) {
                        std::cout << "Deserialized " << received_events->size() << " events\n";
                        std::cout << "Sequence number: " << received_seq << "\n";
                        
                        // Verify first event
                        if (!received_events->empty()) {
                            const auto& first = (*received_events)[0];
                            std::cout << "First event - Module: " 
                                      << static_cast<int>(first->module) 
                                      << ", Channel: " 
                                      << static_cast<int>(first->channel) << "\n";
                        }
                        
                        std::cout << "Benefits of NEW API:\n";
                        std::cout << "  ✓ User controls serialization format\n";
                        std::cout << "  ✓ Transport only handles bytes\n";
                        std::cout << "  ✓ Clear separation of concerns\n";
                        std::cout << "  ✓ Easy to test each component\n";
                    }
                }
            }
        }
        
        publisher.Disconnect();
        subscriber.Disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "Transport error: " << e.what() << "\n";
    }
}

// Example 5: Format detection and mixed streams
void FormatDetectionExample() {
    std::cout << "\n=== Format Detection Example ===\n";
    
    // This demonstrates how to handle mixed EventData and MinimalEventData streams
    std::cout << "Format detection allows automatic handling of different data types\n";
    std::cout << "Use transport.PeekDataType() to check format before receiving\n";
    std::cout << "Use transport.ReceiveAny() for automatic format handling\n";
    
    // In a real application:
    // auto [data, seq] = transport.ReceiveAny();
    // std::visit([](auto&& events) { /* handle events */ }, data);
}

// Performance benchmark
void PerformanceBenchmark() {
    std::cout << "\n=== Performance Benchmark ===\n";
    
    const size_t ITERATIONS = 10;
    const size_t EVENTS_PER_ITERATION = 100000;
    
    std::cout << "Running " << ITERATIONS << " iterations with " 
              << EVENTS_PER_ITERATION << " events each...\n";
    
    double total_creation_time = 0;
    double total_serialization_time = 0;
    
    for (size_t i = 0; i < ITERATIONS; ++i) {
        // Measure creation time
        auto start = std::chrono::high_resolution_clock::now();
        auto events = CreateTestEvents(EVENTS_PER_ITERATION, i % 16);
        auto end = std::chrono::high_resolution_clock::now();
        
        total_creation_time += std::chrono::duration<double>(end - start).count();
        
        // Measure serialization time
        Serializer serializer(FORMAT_VERSION_MINIMAL_EVENTDATA);
        start = std::chrono::high_resolution_clock::now();
        auto encoded = serializer.Encode(events, i);
        end = std::chrono::high_resolution_clock::now();
        
        total_serialization_time += std::chrono::duration<double>(end - start).count();
    }
    
    double avg_creation_time = total_creation_time / ITERATIONS;
    double avg_serialization_time = total_serialization_time / ITERATIONS;
    
    std::cout << "\nResults (average per iteration):\n";
    std::cout << "  Creation: " << (avg_creation_time * 1000) << " ms\n";
    std::cout << "  Creation rate: " << (EVENTS_PER_ITERATION / avg_creation_time) 
              << " events/second\n";
    std::cout << "  Serialization: " << (avg_serialization_time * 1000) << " ms\n";
    std::cout << "  Serialization rate: " << (EVENTS_PER_ITERATION / avg_serialization_time) 
              << " events/second\n";
}

int main(int argc, char* argv[]) {
    std::cout << "DELILA2 MinimalEventData Examples\n";
    std::cout << "==================================\n";
    
    // Run examples
    BasicUsageExample();
    BatchProcessingExample();
    SerializationExample();
    
    // Transport examples - both old and new API
    NetworkTransportExample_OldAPI();
    NetworkTransportExample_NewAPI();
    
    FormatDetectionExample();
    PerformanceBenchmark();
    
    std::cout << "\n=== Examples Complete ===\n";
    std::cout << "Note: Use NEW API for new code. OLD API is deprecated.\n";
    return 0;
}