#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "../../../lib/net/include/ZMQTransport.hpp"
#include "../../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::MinimalEventData;

class ZMQTransportMinimalTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport = std::make_unique<ZMQTransport>();
    }
    
    void TearDown() override {
        if (transport && transport->IsConnected()) {
            transport->Disconnect();
        }
    }
    
    std::unique_ptr<ZMQTransport> transport;
    
    // Helper function to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEvents(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            events->push_back(std::make_unique<MinimalEventData>(
                static_cast<uint8_t>(i % 256),      // module
                static_cast<uint8_t>(i % 64),       // channel  
                static_cast<double>(i * 1000.0),    // timeStampNs
                static_cast<uint16_t>(i * 10),      // energy
                static_cast<uint16_t>(i * 5),       // energyShort
                static_cast<uint64_t>(i % 4)        // flags
            ));
        }
        return events;
    }
    
    // Helper function to configure transport for testing
    void ConfigureTransport() {
        TransportConfig config;
        config.data_address = "tcp://localhost:15555";  // Use different port to avoid conflicts
        config.data_pattern = "PUB";
        config.bind_data = true;
        
        ASSERT_TRUE(transport->Configure(config));
    }
};

// RED: Write failing tests first - these methods don't exist yet
TEST_F(ZMQTransportMinimalTest, SendMinimalMethodExists) {
    ConfigureTransport();
    ASSERT_TRUE(transport->Connect());
    
    auto events = CreateTestMinimalEvents(1);
    
    // This will fail initially - SendMinimal method doesn't exist yet
    EXPECT_TRUE(transport->SendMinimal(events));
}

TEST_F(ZMQTransportMinimalTest, ReceiveMinimalMethodExists) {
    ConfigureTransport();
    ASSERT_TRUE(transport->Connect());
    
    // This will fail initially - ReceiveMinimal method doesn't exist yet
    auto [events, seq] = transport->ReceiveMinimal();
    
    // Should return nullptr when no data available, but method should exist
    EXPECT_EQ(events, nullptr);
    EXPECT_EQ(seq, 0);
}

TEST_F(ZMQTransportMinimalTest, SendMinimalRequiresConfiguration) {
    auto events = CreateTestMinimalEvents(1);
    
    // Should fail without configuration
    EXPECT_FALSE(transport->SendMinimal(events));
}

TEST_F(ZMQTransportMinimalTest, SendMinimalRequiresConnection) {
    ConfigureTransport();
    
    auto events = CreateTestMinimalEvents(1);
    
    // Should fail without connection
    EXPECT_FALSE(transport->SendMinimal(events));
}

TEST_F(ZMQTransportMinimalTest, ReceiveMinimalRequiresConnection) {
    ConfigureTransport();
    
    // Should fail without connection
    auto [events, seq] = transport->ReceiveMinimal();
    EXPECT_EQ(events, nullptr);
    EXPECT_EQ(seq, 0);
}

TEST_F(ZMQTransportMinimalTest, SendMinimalHandlesNullInput) {
    ConfigureTransport();
    ASSERT_TRUE(transport->Connect());
    
    // Should handle null input gracefully
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> null_events = nullptr;
    EXPECT_FALSE(transport->SendMinimal(null_events));
}

TEST_F(ZMQTransportMinimalTest, SendMinimalHandlesEmptyInput) {
    ConfigureTransport();
    ASSERT_TRUE(transport->Connect());
    
    // Should handle empty vector
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    EXPECT_FALSE(transport->SendMinimal(empty_events));
}

// More advanced tests that will pass once basic functionality is implemented
TEST_F(ZMQTransportMinimalTest, RoundTripSingleEvent) {
    // Setup transport pair for round-trip testing
    TransportConfig pub_config;
    pub_config.data_address = "tcp://*:15556";
    pub_config.data_pattern = "PUSH";
    pub_config.bind_data = true;
    
    ZMQTransport publisher;
    ASSERT_TRUE(publisher.Configure(pub_config));
    ASSERT_TRUE(publisher.Connect());
    
    TransportConfig sub_config;
    sub_config.data_address = "tcp://localhost:15556";
    sub_config.data_pattern = "PULL";
    sub_config.bind_data = false;
    
    ZMQTransport subscriber;
    ASSERT_TRUE(subscriber.Configure(sub_config));
    ASSERT_TRUE(subscriber.Connect());
    
    // Allow time for connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create test data
    auto sent_events = CreateTestMinimalEvents(1);
    auto& original = (*sent_events)[0];
    
    // Send data
    EXPECT_TRUE(publisher.SendMinimal(sent_events));
    
    // Receive data
    auto [received_events, seq] = subscriber.ReceiveMinimal();
    
    ASSERT_NE(received_events, nullptr);
    ASSERT_EQ(received_events->size(), 1);
    
    auto& received = (*received_events)[0];
    
    // Verify data integrity
    EXPECT_EQ(received->module, original->module);
    EXPECT_EQ(received->channel, original->channel);
    EXPECT_EQ(received->timeStampNs, original->timeStampNs);
    EXPECT_EQ(received->energy, original->energy);
    EXPECT_EQ(received->energyShort, original->energyShort);
    EXPECT_EQ(received->flags, original->flags);
}

TEST_F(ZMQTransportMinimalTest, RoundTripMultipleEvents) {
    // Setup transport pair
    TransportConfig pub_config;
    pub_config.data_address = "tcp://*:15557";
    pub_config.data_pattern = "PUSH";
    pub_config.bind_data = true;
    
    ZMQTransport publisher;
    ASSERT_TRUE(publisher.Configure(pub_config));
    ASSERT_TRUE(publisher.Connect());
    
    TransportConfig sub_config;
    sub_config.data_address = "tcp://localhost:15557";
    sub_config.data_pattern = "PULL";
    sub_config.bind_data = false;
    
    ZMQTransport subscriber;
    ASSERT_TRUE(subscriber.Configure(sub_config));
    ASSERT_TRUE(subscriber.Connect());
    
    // Allow time for connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test with multiple events
    const size_t EVENT_COUNT = 100;
    auto sent_events = CreateTestMinimalEvents(EVENT_COUNT);
    
    // Send data
    EXPECT_TRUE(publisher.SendMinimal(sent_events));
    
    // Receive data
    auto [received_events, seq] = subscriber.ReceiveMinimal();
    
    ASSERT_NE(received_events, nullptr);
    ASSERT_EQ(received_events->size(), EVENT_COUNT);
    
    // Verify data integrity for all events
    for (size_t i = 0; i < EVENT_COUNT; ++i) {
        const auto& sent = (*sent_events)[i];
        const auto& received = (*received_events)[i];
        
        EXPECT_EQ(received->module, sent->module) << "Event " << i << " module mismatch";
        EXPECT_EQ(received->channel, sent->channel) << "Event " << i << " channel mismatch";
        EXPECT_EQ(received->timeStampNs, sent->timeStampNs) << "Event " << i << " timestamp mismatch";
        EXPECT_EQ(received->energy, sent->energy) << "Event " << i << " energy mismatch";
        EXPECT_EQ(received->energyShort, sent->energyShort) << "Event " << i << " energyShort mismatch";
        EXPECT_EQ(received->flags, sent->flags) << "Event " << i << " flags mismatch";
    }
}

TEST_F(ZMQTransportMinimalTest, SequenceNumberHandling) {
    // Setup transport pair
    TransportConfig pub_config;
    pub_config.data_address = "tcp://*:15558";
    pub_config.data_pattern = "PUSH";
    pub_config.bind_data = true;
    
    ZMQTransport publisher;
    ASSERT_TRUE(publisher.Configure(pub_config));
    ASSERT_TRUE(publisher.Connect());
    
    TransportConfig sub_config;
    sub_config.data_address = "tcp://localhost:15558";
    sub_config.data_pattern = "PULL";
    sub_config.bind_data = false;
    
    ZMQTransport subscriber;
    ASSERT_TRUE(subscriber.Configure(sub_config));
    ASSERT_TRUE(subscriber.Connect());
    
    // Allow time for connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Send multiple batches and verify sequence numbers increment
    for (int batch = 0; batch < 3; ++batch) {
        auto events = CreateTestMinimalEvents(10);
        EXPECT_TRUE(publisher.SendMinimal(events));
        
        auto [received_events, seq] = subscriber.ReceiveMinimal();
        ASSERT_NE(received_events, nullptr);
        EXPECT_GT(seq, 0);  // Sequence number should be positive
        
        // Allow small delay between batches
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}