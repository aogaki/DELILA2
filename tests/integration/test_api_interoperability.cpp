/**
 * @file test_api_interoperability.cpp
 * @brief Integration tests for old and new API interoperability
 * 
 * This test ensures that:
 * 1. Old API senders can be received by new API receivers
 * 2. New API senders can be received by old API receivers  
 * 3. Data integrity is maintained across the APIs
 * 4. Backward compatibility is preserved
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>

#include "../../lib/net/include/ZMQTransport.hpp"
#include "../../lib/net/include/Serializer.hpp"
#include "../../include/delila/core/EventData.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::EventData;
using DELILA::Digitizer::MinimalEventData;
using namespace std::chrono_literals;

class APIInteroperabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use different ports for each test to avoid conflicts
        static int port_counter = 16000;
        current_port_ = port_counter++;
    }
    
    void TearDown() override {
        // Small delay to ensure sockets are properly closed
        std::this_thread::sleep_for(10ms);
    }
    
    // Helper to create test EventData
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>(0); // No waveform
            event->module = static_cast<uint8_t>(i % 256);
            event->channel = static_cast<uint8_t>(i % 64);
            event->timeStampNs = static_cast<double>(i * 1000.0);
            event->energy = static_cast<uint16_t>(1000 + i);
            event->energyShort = static_cast<uint16_t>(500 + i);
            event->flags = static_cast<uint64_t>(i % 4);
            events->push_back(std::move(event));
        }
        return events;
    }
    
    // Helper to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEventData(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            events->push_back(std::make_unique<MinimalEventData>(
                static_cast<uint8_t>(i % 256),      // module
                static_cast<uint8_t>(i % 64),       // channel  
                static_cast<double>(i * 1000.0),    // timeStampNs
                static_cast<uint16_t>(1000 + i),    // energy
                static_cast<uint16_t>(500 + i),     // energyShort
                static_cast<uint64_t>(i % 4)        // flags
            ));
        }
        return events;
    }
    
    // Helper to setup transport
    void SetupTransport(ZMQTransport& transport, bool bind, const std::string& pattern = "PUB") {
        TransportConfig config;
        config.data_address = bind ? 
            "tcp://127.0.0.1:" + std::to_string(current_port_) :
            "tcp://127.0.0.1:" + std::to_string(current_port_);
        config.data_pattern = pattern;
        config.bind_data = bind;
        config.is_publisher = (pattern == "PUB");
        
        ASSERT_TRUE(transport.Configure(config));
        ASSERT_TRUE(transport.Connect());
    }
    
    int current_port_;
};

// Test: Old API sender (EventData) → New API receiver
TEST_F(APIInteroperabilityTest, OldSender_EventData_NewReceiver) {
    ZMQTransport sender, receiver;
    
    // Setup transports
    SetupTransport(sender, true, "PUB");   // Publisher binds
    SetupTransport(receiver, false, "SUB"); // Subscriber connects
    
    // Allow connection to establish
    std::this_thread::sleep_for(100ms);
    
    // Create test data
    auto original_events = CreateTestEventData(5);
    
    // Store original data for comparison
    std::vector<EventData> original_copies;
    for (const auto& event : *original_events) {
        EventData copy(0);
        copy = *event;
        original_copies.push_back(copy);
    }
    
    // OLD API: Send using deprecated method
    ASSERT_TRUE(sender.Send(original_events));
    
    // Give time for delivery
    std::this_thread::sleep_for(50ms);
    
    // NEW API: Receive using byte-based method
    auto received_bytes = receiver.ReceiveBytes();
    ASSERT_TRUE(received_bytes);
    EXPECT_GT(received_bytes->size(), 0);
    
    // Deserialize using external serializer
    Serializer deserializer;
    auto [received_events, sequence_number] = deserializer.Decode(received_bytes);
    
    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), 5);
    EXPECT_GT(sequence_number, 0);  // Should have a sequence number
    
    // Verify data integrity
    for (size_t i = 0; i < received_events->size(); ++i) {
        const auto& received = (*received_events)[i];
        const auto& original = original_copies[i];
        
        EXPECT_EQ(received->module, original.module);
        EXPECT_EQ(received->channel, original.channel);
        EXPECT_DOUBLE_EQ(received->timeStampNs, original.timeStampNs);
        EXPECT_EQ(received->energy, original.energy);
        EXPECT_EQ(received->energyShort, original.energyShort);
        EXPECT_EQ(received->flags, original.flags);
    }
}

// Test: New API sender → Old API receiver (EventData)
TEST_F(APIInteroperabilityTest, NewSender_OldReceiver_EventData) {
    ZMQTransport sender, receiver;
    
    // Setup transports
    SetupTransport(sender, true, "PUB");   // Publisher binds
    SetupTransport(receiver, false, "SUB"); // Subscriber connects
    
    // Allow connection to establish
    std::this_thread::sleep_for(100ms);
    
    // Create test data
    auto original_events = CreateTestEventData(5);
    
    // Store original data for comparison
    std::vector<EventData> original_copies;
    for (const auto& event : *original_events) {
        EventData copy(0);
        copy = *event;
        original_copies.push_back(copy);
    }
    
    // NEW API: Serialize and send bytes
    Serializer serializer;
    uint64_t test_sequence = 42;
    auto serialized_bytes = serializer.Encode(original_events, test_sequence);
    ASSERT_TRUE(serialized_bytes);
    
    ASSERT_TRUE(sender.SendBytes(serialized_bytes));
    
    // Give time for delivery
    std::this_thread::sleep_for(50ms);
    
    // OLD API: Receive using deprecated method
    auto [received_events, sequence_number] = receiver.Receive();
    
    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), 5);
    EXPECT_EQ(sequence_number, test_sequence);
    
    // Verify data integrity
    for (size_t i = 0; i < received_events->size(); ++i) {
        const auto& received = (*received_events)[i];
        const auto& original = original_copies[i];
        
        EXPECT_EQ(received->module, original.module);
        EXPECT_EQ(received->channel, original.channel);
        EXPECT_DOUBLE_EQ(received->timeStampNs, original.timeStampNs);
        EXPECT_EQ(received->energy, original.energy);
        EXPECT_EQ(received->energyShort, original.energyShort);
        EXPECT_EQ(received->flags, original.flags);
    }
}

// Test: Old API sender (MinimalEventData) → New API receiver
TEST_F(APIInteroperabilityTest, OldSender_MinimalEventData_NewReceiver) {
    ZMQTransport sender, receiver;
    
    // Setup transports
    SetupTransport(sender, true, "PUB");
    SetupTransport(receiver, false, "SUB");
    
    std::this_thread::sleep_for(100ms);
    
    // Create test data
    auto original_events = CreateTestMinimalEventData(5);
    
    // Store original data for comparison
    std::vector<MinimalEventData> original_copies;
    for (const auto& event : *original_events) {
        original_copies.push_back(*event);
    }
    
    // OLD API: Send using deprecated method
    ASSERT_TRUE(sender.SendMinimal(original_events));
    
    std::this_thread::sleep_for(50ms);
    
    // NEW API: Receive as bytes and deserialize
    auto received_bytes = receiver.ReceiveBytes();
    ASSERT_TRUE(received_bytes);
    
    Serializer deserializer;
    auto [received_events, sequence_number] = deserializer.Decode(received_bytes);
    
    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), 5);
    
    // Verify data integrity
    for (size_t i = 0; i < received_events->size(); ++i) {
        const auto& received = (*received_events)[i];
        const auto& original = original_copies[i];
        
        EXPECT_EQ(received->module, original.module);
        EXPECT_EQ(received->channel, original.channel);
        EXPECT_DOUBLE_EQ(received->timeStampNs, original.timeStampNs);
        EXPECT_EQ(received->energy, original.energy);
        EXPECT_EQ(received->energyShort, original.energyShort);
        EXPECT_EQ(received->flags, original.flags);
    }
}

// Test: New API sender → Old API receiver (MinimalEventData)
TEST_F(APIInteroperabilityTest, NewSender_OldReceiver_MinimalEventData) {
    ZMQTransport sender, receiver;
    
    // Setup transports
    SetupTransport(sender, true, "PUB");
    SetupTransport(receiver, false, "SUB");
    
    std::this_thread::sleep_for(100ms);
    
    // Create test data
    auto original_events = CreateTestMinimalEventData(5);
    
    // Store original data for comparison
    std::vector<MinimalEventData> original_copies;
    for (const auto& event : *original_events) {
        original_copies.push_back(*event);
    }
    
    // NEW API: Serialize and send
    Serializer serializer;
    uint64_t test_sequence = 123;
    auto serialized_bytes = serializer.Encode(original_events, test_sequence);
    ASSERT_TRUE(serialized_bytes);
    
    ASSERT_TRUE(sender.SendBytes(serialized_bytes));
    
    std::this_thread::sleep_for(50ms);
    
    // OLD API: Receive using deprecated method
    auto [received_events, sequence_number] = receiver.ReceiveMinimal();
    
    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), 5);
    EXPECT_EQ(sequence_number, test_sequence);
    
    // Verify data integrity
    for (size_t i = 0; i < received_events->size(); ++i) {
        const auto& received = (*received_events)[i];
        const auto& original = original_copies[i];
        
        EXPECT_EQ(received->module, original.module);
        EXPECT_EQ(received->channel, original.channel);
        EXPECT_DOUBLE_EQ(received->timeStampNs, original.timeStampNs);
        EXPECT_EQ(received->energy, original.energy);
        EXPECT_EQ(received->energyShort, original.energyShort);
        EXPECT_EQ(received->flags, original.flags);
    }
}

// Test: Data type detection works across APIs
TEST_F(APIInteroperabilityTest, DataTypeDetection_Interoperability) {
    ZMQTransport sender, receiver;
    
    // Setup transports
    SetupTransport(sender, true, "PUB");
    SetupTransport(receiver, false, "SUB");
    
    std::this_thread::sleep_for(100ms);
    
    // Test 1: Send EventData via OLD API, detect via NEW API
    {
        auto events = CreateTestEventData(3);
        ASSERT_TRUE(sender.Send(events));
        
        std::this_thread::sleep_for(50ms);
        
        // Data type detection should work regardless of API used to send
        auto data_type = receiver.PeekDataType();
        EXPECT_EQ(data_type, ZMQTransport::DataType::EVENTDATA);
        
        // Consume the data to clear the queue
        auto received_bytes = receiver.ReceiveBytes();
        EXPECT_TRUE(received_bytes);
    }
    
    // Test 2: Send MinimalEventData via NEW API, detect via OLD API
    {
        auto events = CreateTestMinimalEventData(3);
        Serializer serializer;
        auto bytes = serializer.Encode(events, 456);
        ASSERT_TRUE(bytes);
        ASSERT_TRUE(sender.SendBytes(bytes));
        
        std::this_thread::sleep_for(50ms);
        
        // Data type detection should work regardless of API used to receive
        auto data_type = receiver.PeekDataType();
        EXPECT_EQ(data_type, ZMQTransport::DataType::MINIMAL_EVENTDATA);
        
        // Consume the data
        auto [received_events, seq] = receiver.ReceiveMinimal();
        EXPECT_TRUE(received_events);
    }
}

// Test: Mixed API usage in same application
TEST_F(APIInteroperabilityTest, MixedAPIUsage_SameApplication) {
    ZMQTransport transport;
    
    // Setup bidirectional transport (for this test, we'll use multiple operations)
    SetupTransport(transport, true, "PUB");
    
    Serializer serializer;
    uint64_t sequence = 1;
    
    // Mix OLD and NEW API calls on same transport
    {
        // NEW API
        auto events1 = CreateTestEventData(2);
        auto bytes1 = serializer.Encode(events1, sequence++);
        EXPECT_TRUE(transport.SendBytes(bytes1));
    }
    
    {
        // OLD API (deprecated but should still work)
        auto events2 = CreateTestMinimalEventData(3);
        EXPECT_TRUE(transport.SendMinimal(events2));
    }
    
    {
        // NEW API again
        auto events3 = CreateTestEventData(1);
        auto bytes3 = serializer.Encode(events3, sequence++);
        EXPECT_TRUE(transport.SendBytes(bytes3));
    }
    
    // All operations should succeed, demonstrating API compatibility
}

// Test: Error handling consistency across APIs
TEST_F(APIInteroperabilityTest, ErrorHandling_Consistency) {
    ZMQTransport transport;
    // Intentionally don't configure the transport
    
    // Both APIs should handle errors consistently
    {
        auto events = CreateTestEventData(1);
        EXPECT_FALSE(transport.Send(events));  // OLD API
    }
    
    {
        Serializer serializer;
        auto events = CreateTestEventData(1);
        auto bytes = serializer.Encode(events, 1);
        EXPECT_FALSE(transport.SendBytes(bytes));  // NEW API
    }
    
    // Both should fail gracefully with unconfigured transport
}

// Performance comparison test
TEST_F(APIInteroperabilityTest, Performance_Comparison) {
    ZMQTransport sender, receiver;
    
    SetupTransport(sender, true, "PUB");
    SetupTransport(receiver, false, "SUB");
    
    std::this_thread::sleep_for(100ms);
    
    const size_t EVENT_COUNT = 100;
    const size_t ITERATIONS = 10;
    
    // Measure OLD API performance
    auto start_old = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        auto events = CreateTestEventData(EVENT_COUNT);
        EXPECT_TRUE(sender.Send(events));
        
        // Receive to keep queue empty
        std::this_thread::sleep_for(5ms);
        auto [received, seq] = receiver.Receive();
        EXPECT_TRUE(received);
    }
    auto end_old = std::chrono::high_resolution_clock::now();
    
    // Measure NEW API performance
    Serializer serializer;
    uint64_t sequence = 1;
    
    auto start_new = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        auto events = CreateTestEventData(EVENT_COUNT);
        auto bytes = serializer.Encode(events, sequence++);
        EXPECT_TRUE(sender.SendBytes(bytes));
        
        // Receive to keep queue empty
        std::this_thread::sleep_for(5ms);
        auto received_bytes = receiver.ReceiveBytes();
        EXPECT_TRUE(received_bytes);
    }
    auto end_new = std::chrono::high_resolution_clock::now();
    
    auto old_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_old - start_old);
    auto new_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_new - start_new);
    
    // Performance should be comparable (within 50% difference)
    double ratio = static_cast<double>(new_duration.count()) / old_duration.count();
    EXPECT_LT(ratio, 1.5);  // NEW API shouldn't be more than 50% slower
    
    std::cout << "Performance comparison (lower is better):\n";
    std::cout << "  OLD API: " << old_duration.count() << " μs\n";
    std::cout << "  NEW API: " << new_duration.count() << " μs\n";
    std::cout << "  Ratio: " << ratio << "\n";
}