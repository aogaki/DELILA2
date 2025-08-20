/**
 * @file separated_serialization.cpp
 * @brief Example demonstrating separated serialization in DELILA2 Network Layer
 * 
 * This example shows how to:
 * - Use the new byte-based ZMQTransport interface (SendBytes/ReceiveBytes)
 * - Manually control serialization with external Serializer
 * - Compare old vs new API approaches
 * - Demonstrate the benefits of separation of concerns
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

#include "ZMQTransport.hpp"
#include "Serializer.hpp"
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
        event->module = 1;
        event->channel = static_cast<uint8_t>(i % 16);
        event->timeStampNs = 1000000.0 + i * 1000;
        event->energy = 1000 + (i * 10);
        event->energyShort = 500 + (i * 5);
        events->push_back(std::move(event));
    }
    
    return events;
}

void DemonstrateOldAPI() {
    std::cout << "\n=== OLD API (Deprecated) ===\n";
    std::cout << "Serialization is handled internally by ZMQTransport\n";
    
    try {
        // Configure transport 
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://127.0.0.1:5555";
        config.bind_data = true;
        config.data_pattern = "PUSH";
        config.is_publisher = true;
        
        if (!transport.Configure(config)) {
            std::cerr << "Failed to configure transport\n";
            return;
        }
        
        if (!transport.Connect()) {
            std::cerr << "Failed to connect transport\n";
            return;
        }
        
        // OLD WAY: Transport handles serialization internally
        auto events = CreateTestEvents(5);
        
        std::cout << "Using deprecated Send() method...\n";
        bool sent = transport.Send(events);  // [[deprecated]] method
        
        if (sent) {
            std::cout << "✓ Sent " << events->size() << " events using old API\n";
            std::cout << "  NOTE: This creates an internal serializer inside ZMQTransport\n";
            std::cout << "  NOTE: Sequence number is managed by transport layer\n";
            std::cout << "  NOTE: You have no control over serialization format\n";
        } else {
            std::cout << "✗ Failed to send events\n";
        }
        
        transport.Disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "Old API error: " << e.what() << "\n";
    }
}

void DemonstrateNewAPI() {
    std::cout << "\n=== NEW API (Recommended) ===\n";
    std::cout << "Serialization is handled externally by user code\n";
    
    try {
        // Configure transport (same as before)
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://127.0.0.1:5556";
        config.bind_data = true;
        config.data_pattern = "PUSH";
        config.is_publisher = true;
        
        if (!transport.Configure(config)) {
            std::cerr << "Failed to configure transport\n";
            return;
        }
        
        if (!transport.Connect()) {
            std::cerr << "Failed to connect transport\n";
            return;
        }
        
        // NEW WAY: Explicit external serialization
        Serializer serializer;
        uint64_t sequence_number = 1;
        
        auto events = CreateTestEvents(5);
        
        std::cout << "Step 1: Serializing events externally...\n";
        auto serialized_bytes = serializer.Encode(events, sequence_number);
        
        if (serialized_bytes) {
            std::cout << "✓ Serialized " << events->size() << " events into " 
                      << serialized_bytes->size() << " bytes\n";
            std::cout << "  - Sequence number: " << sequence_number << "\n";
            std::cout << "  - User controls serialization format and sequence\n";
            
            std::cout << "Step 2: Sending raw bytes via transport...\n";
            bool sent = transport.SendBytes(serialized_bytes);
            
            if (sent) {
                std::cout << "✓ Sent serialized data via transport\n";
                std::cout << "  - Transport only handles byte transmission\n";
                std::cout << "  - Clear separation of concerns\n";
                std::cout << "  - Ownership of bytes transferred to transport\n";
            } else {
                std::cout << "✗ Failed to send bytes\n";
            }
        } else {
            std::cout << "✗ Failed to serialize events\n";
        }
        
        transport.Disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "New API error: " << e.what() << "\n";
    }
}

void DemonstrateReceiving() {
    std::cout << "\n=== RECEIVING WITH NEW API ===\n";
    
    try {
        // Setup sender
        ZMQTransport sender;
        TransportConfig sender_config;
        sender_config.data_address = "tcp://127.0.0.1:5557";
        sender_config.bind_data = true;
        sender_config.data_pattern = "PUSH";
        sender_config.is_publisher = true;
        
        sender.Configure(sender_config);
        sender.Connect();
        
        // Setup receiver
        ZMQTransport receiver;
        TransportConfig receiver_config;
        receiver_config.data_address = "tcp://127.0.0.1:5557";
        receiver_config.bind_data = false;  // Connect to sender
        receiver_config.data_pattern = "PULL";
        receiver_config.is_publisher = false;
        
        receiver.Configure(receiver_config);
        receiver.Connect();
        
        // Give connections time to establish
        std::this_thread::sleep_for(100ms);
        
        // Send data
        Serializer sender_serializer;
        uint64_t seq = 42;
        auto events = CreateTestEvents(3);
        
        std::cout << "Sending data...\n";
        auto bytes = sender_serializer.Encode(events, seq);
        if (bytes && sender.SendBytes(bytes)) {
            std::cout << "✓ Sent " << events->size() << " events\n";
        }
        
        // Receive data
        std::this_thread::sleep_for(50ms);  // Give time for delivery
        
        std::cout << "Receiving data...\n";
        auto received_bytes = receiver.ReceiveBytes();
        
        if (received_bytes) {
            std::cout << "✓ Received " << received_bytes->size() << " bytes\n";
            
            // Deserialize
            Serializer receiver_serializer;
            auto [received_events, received_seq] = receiver_serializer.Decode(received_bytes);
            
            if (received_events) {
                std::cout << "✓ Deserialized " << received_events->size() 
                          << " events (seq: " << received_seq << ")\n";
                
                // Show first event details
                if (!received_events->empty()) {
                    const auto& first_event = (*received_events)[0];
                    std::cout << "  First event: channel=" << static_cast<int>(first_event->channel)
                              << ", energy=" << first_event->energy << "\n";
                }
            } else {
                std::cout << "✗ Failed to deserialize events\n";
            }
        } else {
            std::cout << "✗ No data received (this is expected in some cases)\n";
        }
        
        sender.Disconnect();
        receiver.Disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "Receiving demo error: " << e.what() << "\n";
    }
}

void DemonstrateAdvancedUsage() {
    std::cout << "\n=== ADVANCED USAGE ===\n";
    std::cout << "Multiple serializers, custom sequence management, etc.\n";
    
    try {
        ZMQTransport transport;
        TransportConfig config;
        config.data_address = "tcp://127.0.0.1:5558";
        config.bind_data = true;
        config.data_pattern = "PUSH";
        config.is_publisher = true;
        
        transport.Configure(config);
        transport.Connect();
        
        // Multiple serializers for different purposes
        Serializer high_precision_serializer;
        Serializer compact_serializer;
        
        // Custom sequence number management
        uint64_t global_sequence = 1000;
        
        // Send different types of data
        std::cout << "Sending EventData with high precision serializer...\n";
        auto full_events = CreateTestEvents(2);
        auto full_bytes = high_precision_serializer.Encode(full_events, global_sequence++);
        if (full_bytes && transport.SendBytes(full_bytes)) {
            std::cout << "✓ Sent full EventData (seq: " << (global_sequence-1) << ")\n";
        }
        
        std::cout << "Sending MinimalEventData with compact serializer...\n";
        auto minimal_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        for (int i = 0; i < 3; ++i) {
            auto event = std::make_unique<MinimalEventData>();
            event->channel = static_cast<uint8_t>(i);
            event->timeStampNs = 2000000.0 + i * 1000;
            event->energy = static_cast<uint16_t>(1500 + i * 20);
            minimal_events->push_back(std::move(event));
        }
        
        auto minimal_bytes = compact_serializer.Encode(minimal_events, global_sequence++);
        if (minimal_bytes && transport.SendBytes(minimal_bytes)) {
            std::cout << "✓ Sent minimal EventData (seq: " << (global_sequence-1) << ")\n";
        }
        
        std::cout << "Benefits of new approach:\n";
        std::cout << "  - Different serializers for different data types\n";
        std::cout << "  - Custom sequence number schemes\n";
        std::cout << "  - User controls serialization format\n";
        std::cout << "  - Transport layer is pure and simple\n";
        std::cout << "  - Easy to test and mock\n";
        
        transport.Disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "Advanced usage error: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== DELILA2 Separated Serialization Example ===\n";
    std::cout << "This example demonstrates the refactored ZMQTransport architecture\n";
    std::cout << "where serialization is separated from transport.\n";
    
    // Show both approaches
    DemonstrateOldAPI();
    DemonstrateNewAPI();
    DemonstrateReceiving();
    DemonstrateAdvancedUsage();
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "OLD API (deprecated):\n";
    std::cout << "  ❌ Transport tightly coupled to serialization\n";
    std::cout << "  ❌ Internal sequence number management\n";
    std::cout << "  ❌ Hard to test and customize\n";
    std::cout << "  ❌ Violates Single Responsibility Principle\n";
    
    std::cout << "\nNEW API (recommended):\n";
    std::cout << "  ✅ Clear separation of concerns\n";
    std::cout << "  ✅ User controls serialization\n";
    std::cout << "  ✅ Easy to test transport and serialization separately\n";
    std::cout << "  ✅ Multiple serializers possible\n";
    std::cout << "  ✅ Zero-copy optimization with ownership transfer\n";
    std::cout << "  ✅ Transport layer is pure byte transport\n";
    
    std::cout << "\nMigration path:\n";
    std::cout << "  1. Replace Send(events) with Encode() + SendBytes()\n";
    std::cout << "  2. Replace Receive() with ReceiveBytes() + Decode()\n";
    std::cout << "  3. Manage sequence numbers in your application\n";
    std::cout << "  4. Consider multiple serializers for different use cases\n";
    
    return 0;
}