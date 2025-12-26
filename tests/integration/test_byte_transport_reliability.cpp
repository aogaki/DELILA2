/**
 * @file test_byte_transport_reliability.cpp
 * @brief Integration tests for byte-based transport reliability
 * 
 * This test ensures that:
 * 1. DataProcessor + ZMQTransport byte-based transport is reliable
 * 2. Data integrity is maintained across network transport
 * 3. Different data types are handled correctly
 * 4. Large data sets can be transported successfully
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

#include "../../lib/net/include/ZMQTransport.hpp"
#include "../../lib/net/include/DataProcessor.hpp"
#include "../../include/delila/core/EventData.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::EventData;
using DELILA::Digitizer::MinimalEventData;
using namespace std::chrono_literals;

// Port manager to avoid conflicts between tests
class IntegrationPortManager {
private:
    static std::atomic<int> next_port_;
public:
    static int GetNextPort() { 
        return next_port_.fetch_add(1); 
    }
};

// Initialize starting from a different range than unit tests
std::atomic<int> IntegrationPortManager::next_port_{36000};

class ByteTransportReliabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use dynamic port allocation to avoid conflicts
        current_port_ = IntegrationPortManager::GetNextPort();
    }
    
    void TearDown() override {
        // Increased delay to ensure sockets are properly closed
        std::this_thread::sleep_for(100ms);
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
    
    // Helper to setup transport with improved error handling
    void SetupTransport(ZMQTransport& transport, bool bind, const std::string& pattern = "PUB") {
        TransportConfig config;
        config.data_address = "tcp://127.0.0.1:" + std::to_string(current_port_);
        config.data_pattern = pattern;
        config.bind_data = bind;
        config.is_publisher = (pattern == "PUB");
        // Disable status socket by setting same address as data
        config.status_address = config.data_address;
        // Disable command socket
        config.command_address = "";

        ASSERT_TRUE(transport.Configure(config));

        // For reliable connection, bind first, then connect with delay
        ASSERT_TRUE(transport.Connect());

        // Additional delay for socket setup, especially important for integration tests
        if (bind) {
            std::this_thread::sleep_for(200ms);  // Binder needs time to be ready
        } else {
            std::this_thread::sleep_for(100ms);  // Connector delay
        }
    }
    
    int current_port_;
};

// Test: Basic EventData transport reliability
TEST_F(ByteTransportReliabilityTest, EventData_Transport_Reliability) {
    ZMQTransport sender, receiver;
    DataProcessor sender_processor, receiver_processor;
    
    // Setup transports - bind first, connect second with proper delays
    SetupTransport(sender, true, "PUB");   // Publisher binds (includes 200ms delay)
    SetupTransport(receiver, false, "SUB"); // Subscriber connects (includes 100ms delay)
    
    // Additional connection stabilization time for pub/sub pattern
    std::this_thread::sleep_for(200ms);
    
    // Create test data
    auto original_events = CreateTestEventData(10);
    
    // Store original data for comparison
    std::vector<EventData> original_copies;
    for (const auto& event : *original_events) {
        EventData copy(0);
        copy = *event;
        original_copies.push_back(copy);
    }
    
    // Process and send using byte-based transport
    uint64_t test_sequence = 42;
    auto serialized_bytes = sender_processor.Process(original_events, test_sequence);
    ASSERT_TRUE(serialized_bytes);
    
    ASSERT_TRUE(sender.SendBytes(serialized_bytes));
    
    // Give time for delivery
    std::this_thread::sleep_for(50ms);
    
    // Receive and decode using byte-based transport
    auto received_bytes = receiver.ReceiveBytes();
    ASSERT_TRUE(received_bytes);
    EXPECT_GT(received_bytes->size(), 0);
    
    auto [received_events, sequence_number] = receiver_processor.Decode(received_bytes);
    
    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), 10);
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

// Test: MinimalEventData transport reliability
TEST_F(ByteTransportReliabilityTest, MinimalEventData_Transport_Reliability) {
    ZMQTransport sender, receiver;
    DataProcessor sender_processor, receiver_processor;
    
    // Setup transports
    SetupTransport(sender, true, "PUB");
    SetupTransport(receiver, false, "SUB");
    
    std::this_thread::sleep_for(100ms);
    
    // Create test data
    auto original_events = CreateTestMinimalEventData(15);
    
    // Store original data for comparison
    std::vector<MinimalEventData> original_copies;
    for (const auto& event : *original_events) {
        original_copies.push_back(*event);
    }
    
    // Process and send using byte-based transport
    uint64_t test_sequence = 123;
    auto serialized_bytes = sender_processor.Process(original_events, test_sequence);
    ASSERT_TRUE(serialized_bytes);
    
    ASSERT_TRUE(sender.SendBytes(serialized_bytes));
    
    std::this_thread::sleep_for(50ms);
    
    // Receive and decode using byte-based transport
    auto received_bytes = receiver.ReceiveBytes();
    ASSERT_TRUE(received_bytes);
    
    auto [received_events, sequence_number] = receiver_processor.DecodeMinimal(received_bytes);

    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), 15);
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

// Test: Large data set transport
TEST_F(ByteTransportReliabilityTest, Large_Dataset_Transport) {
    ZMQTransport sender, receiver;
    DataProcessor sender_processor, receiver_processor;
    
    // Setup transports using PUSH/PULL for better large data handling
    SetupTransport(sender, true, "PUSH");
    SetupTransport(receiver, false, "PULL");
    
    std::this_thread::sleep_for(100ms);
    
    // Create large test data set
    const size_t LARGE_COUNT = 1000;
    auto original_events = CreateTestEventData(LARGE_COUNT);
    
    // Process and send
    uint64_t test_sequence = 999;
    auto serialized_bytes = sender_processor.Process(original_events, test_sequence);
    ASSERT_TRUE(serialized_bytes);
    size_t expected_size = serialized_bytes->size();
    EXPECT_GT(expected_size, 10000); // Should be substantial size

    ASSERT_TRUE(sender.SendBytes(serialized_bytes));
    // Note: serialized_bytes is now nullptr after SendBytes

    // Give more time for large data delivery
    std::this_thread::sleep_for(200ms);

    // Receive and decode
    auto received_bytes = receiver.ReceiveBytes();
    ASSERT_TRUE(received_bytes);
    EXPECT_EQ(received_bytes->size(), expected_size);
    
    auto [received_events, sequence_number] = receiver_processor.Decode(received_bytes);
    
    ASSERT_TRUE(received_events);
    EXPECT_EQ(received_events->size(), LARGE_COUNT);
    EXPECT_EQ(sequence_number, test_sequence);
    
    // Spot-check first, middle, and last events
    std::vector<size_t> check_indices = {0, LARGE_COUNT/2, LARGE_COUNT-1};
    for (size_t idx : check_indices) {
        const auto& received = (*received_events)[idx];
        EXPECT_EQ(received->module, static_cast<uint8_t>(idx % 256));
        EXPECT_EQ(received->channel, static_cast<uint8_t>(idx % 64));
        EXPECT_DOUBLE_EQ(received->timeStampNs, static_cast<double>(idx * 1000.0));
        EXPECT_EQ(received->energy, static_cast<uint16_t>(1000 + idx));
    }
}

// Test: Multiple sequential transports
TEST_F(ByteTransportReliabilityTest, Multiple_Sequential_Transports) {
    ZMQTransport sender, receiver;
    DataProcessor sender_processor, receiver_processor;
    
    SetupTransport(sender, true, "PUSH");
    SetupTransport(receiver, false, "PULL");
    
    std::this_thread::sleep_for(100ms);
    
    const int NUM_BATCHES = 5;
    const int EVENTS_PER_BATCH = 20;
    
    for (int batch = 0; batch < NUM_BATCHES; ++batch) {
        // Create batch-specific test data
        auto events = CreateTestEventData(EVENTS_PER_BATCH);
        uint64_t batch_sequence = 1000 + batch;
        
        // Send batch
        auto serialized_bytes = sender_processor.Process(events, batch_sequence);
        ASSERT_TRUE(serialized_bytes);
        ASSERT_TRUE(sender.SendBytes(serialized_bytes));
        
        // Receive batch
        std::this_thread::sleep_for(50ms);
        auto received_bytes = receiver.ReceiveBytes();
        ASSERT_TRUE(received_bytes);
        
        auto [received_events, sequence_number] = receiver_processor.Decode(received_bytes);
        ASSERT_TRUE(received_events);
        EXPECT_EQ(received_events->size(), EVENTS_PER_BATCH);
        EXPECT_EQ(sequence_number, batch_sequence);
        
        // Verify first event of each batch
        const auto& first_event = (*received_events)[0];
        EXPECT_EQ(first_event->energy, 1000);  // First event always has energy 1000
    }
}

// Test: Error handling and edge cases
TEST_F(ByteTransportReliabilityTest, Error_Handling) {
    ZMQTransport transport;
    DataProcessor processor;
    
    // Test sending without configuration should fail
    {
        auto events = CreateTestEventData(1);
        auto bytes = processor.Process(events, 1);
        EXPECT_FALSE(transport.SendBytes(bytes));
    }
    
    // Test receiving without connection should return null
    {
        auto bytes = transport.ReceiveBytes();
        EXPECT_EQ(bytes, nullptr);
    }
    
    // Test with null data should fail
    {
        SetupTransport(transport, true, "PUB");
        std::unique_ptr<std::vector<uint8_t>> null_bytes;
        EXPECT_FALSE(transport.SendBytes(null_bytes));
    }
}