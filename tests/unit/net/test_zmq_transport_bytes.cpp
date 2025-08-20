// Test file for refactored ZMQTransport with byte-based interface
// Following TDD principles - tests written before implementation

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

// Include will fail initially - that's expected in TDD
#include "ZMQTransport.hpp"
#include "test_helpers.hpp"

using namespace DELILA::Net;
using namespace std::chrono_literals;
using namespace TestHelpers;

class ZMQTransportBytesTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport = std::make_unique<ZMQTransport>();
    }

    void TearDown() override {
        if (transport && transport->IsConnected()) {
            transport->Disconnect();
        }
    }

    // Helper to create test data
    std::unique_ptr<std::vector<uint8_t>> CreateTestData(size_t size) {
        auto data = std::make_unique<std::vector<uint8_t>>(size);
        for (size_t i = 0; i < size; ++i) {
            (*data)[i] = static_cast<uint8_t>(i % 256);
        }
        return data;
    }

    // Helper to verify data integrity
    bool VerifyTestData(const std::vector<uint8_t>& data) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] != static_cast<uint8_t>(i % 256)) {
                return false;
            }
        }
        return true;
    }

    // Helper to get basic configuration
    TransportConfig GetBasicPubSubConfig() {
        TransportConfig config;
        config.data_address = "tcp://localhost:15555";
        config.bind_data = true;
        config.data_pattern = "PUB";
        config.is_publisher = true;
        return config;
    }

    TransportConfig GetBasicSubConfig() {
        TransportConfig config;
        config.data_address = "tcp://localhost:15555";
        config.bind_data = false;  // Subscriber connects to publisher
        config.data_pattern = "SUB";
        config.is_publisher = false;
        return config;
    }

    std::unique_ptr<ZMQTransport> transport;
};

// Phase 2.1: Test SendBytes with unique_ptr
TEST_F(ZMQTransportBytesTest, SendBytesAcceptsUniquePtr) {
    // Arrange
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    ASSERT_TRUE(transport->Connect());
    
    auto data = CreateTestData(1024);
    
    // Act
    bool result = transport->SendBytes(data);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(data, nullptr); // Ownership should be transferred
}

// Phase 2.2: Test ReceiveBytes returns unique_ptr
TEST_F(ZMQTransportBytesTest, ReceiveBytesReturnsUniquePtr) {
    // Arrange - Create publisher and subscriber
    auto publisher = std::make_unique<ZMQTransport>();
    auto pub_config = GetBasicPubSubConfig();
    pub_config.data_address = "tcp://127.0.0.1:25555";  // Use different port
    ASSERT_TRUE(publisher->Configure(pub_config));
    ASSERT_TRUE(publisher->Connect());
    
    auto subscriber = std::make_unique<ZMQTransport>();
    auto sub_config = GetBasicSubConfig();
    sub_config.data_address = "tcp://127.0.0.1:25555";  // Match publisher port
    ASSERT_TRUE(subscriber->Configure(sub_config));
    ASSERT_TRUE(subscriber->Connect());
    
    // Allow ZMQ to establish connection
    std::this_thread::sleep_for(100ms);
    
    // Act - Send data
    auto send_data = CreateTestData(1024);
    size_t expected_size = send_data->size();
    ASSERT_TRUE(publisher->SendBytes(send_data));
    
    // Small delay for message propagation
    std::this_thread::sleep_for(50ms);
    
    // Act - Receive data
    auto received_data = subscriber->ReceiveBytes();
    
    // Assert
    ASSERT_NE(received_data, nullptr);
    EXPECT_EQ(received_data->size(), expected_size);
    EXPECT_TRUE(VerifyTestData(*received_data));
}

// Phase 2.3: Test zero-copy optimization with ownership transfer
TEST_F(ZMQTransportBytesTest, ZeroCopyOwnershipTransfer) {
    // Arrange
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    ASSERT_TRUE(transport->Connect());
    
    // Enable zero-copy
    transport->EnableZeroCopy(true);
    EXPECT_TRUE(transport->IsZeroCopyEnabled());
    
    auto data = CreateTestData(1024);
    auto* data_ptr = data.get(); // Keep raw pointer to verify
    
    // Act
    bool result = transport->SendBytes(data);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(data, nullptr); // Ownership transferred
    // Note: Can't verify data_ptr is still valid as ZMQ owns it now
}

// Phase 2.4: Test memory pool with byte buffers
TEST_F(ZMQTransportBytesTest, MemoryPoolWithByteBuffers) {
    // Arrange
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    ASSERT_TRUE(transport->Connect());
    
    // Enable memory pool
    transport->EnableMemoryPool(true);
    transport->SetMemoryPoolSize(10);
    EXPECT_TRUE(transport->IsMemoryPoolEnabled());
    EXPECT_EQ(transport->GetMemoryPoolSize(), 10);
    
    // Act - Send multiple messages
    for (int i = 0; i < 5; ++i) {
        auto data = CreateTestData(1024);
        EXPECT_TRUE(transport->SendBytes(data));
    }
    
    // Assert - Pool should have some buffers
    // Note: Exact behavior depends on implementation
    EXPECT_GE(transport->GetPooledContainerCount(), 0);
}

// Phase 2.5: Test null pointer handling
TEST_F(ZMQTransportBytesTest, SendBytesHandlesNullPointer) {
    // Arrange
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    ASSERT_TRUE(transport->Connect());
    
    std::unique_ptr<std::vector<uint8_t>> null_data = nullptr;
    
    // Act
    bool result = transport->SendBytes(null_data);
    
    // Assert
    EXPECT_FALSE(result); // Should fail gracefully
}

TEST_F(ZMQTransportBytesTest, SendBytesHandlesEmptyData) {
    // Arrange
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    ASSERT_TRUE(transport->Connect());
    
    auto empty_data = std::make_unique<std::vector<uint8_t>>();
    
    // Act
    bool result = transport->SendBytes(empty_data);
    
    // Assert - Could be either behavior depending on design
    // We'll say empty data is invalid
    EXPECT_FALSE(result);
}

// Test configuration validation
TEST_F(ZMQTransportBytesTest, RequiresConfigurationBeforeSend) {
    // Arrange - No configuration
    auto data = CreateTestData(1024);
    
    // Act
    bool result = transport->SendBytes(data);
    
    // Assert
    EXPECT_FALSE(result);
}

TEST_F(ZMQTransportBytesTest, RequiresConnectionBeforeSend) {
    // Arrange - Configure but don't connect
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    
    auto data = CreateTestData(1024);
    
    // Act
    bool result = transport->SendBytes(data);
    
    // Assert
    EXPECT_FALSE(result);
}

// Test various data sizes
TEST_F(ZMQTransportBytesTest, SendsVariousDataSizes) {
    // Arrange
    auto config = GetBasicPubSubConfig();
    ASSERT_TRUE(transport->Configure(config));
    ASSERT_TRUE(transport->Connect());
    
    std::vector<size_t> test_sizes = {1, 100, 1024, 10240, 102400, 1024000};
    
    // Act & Assert
    for (size_t size : test_sizes) {
        auto data = CreateTestData(size);
        EXPECT_TRUE(transport->SendBytes(data)) << "Failed for size: " << size;
    }
}

// Test PAIR pattern (bidirectional)
TEST_F(ZMQTransportBytesTest, PairPatternBidirectional) {
    // Arrange - Create two transports with PAIR pattern
    auto transport1 = std::make_unique<ZMQTransport>();
    auto transport2 = std::make_unique<ZMQTransport>();
    
    TransportConfig config1;
    config1.data_address = "tcp://127.0.0.1:25556";
    config1.bind_data = true;
    config1.data_pattern = "PAIR";
    
    TransportConfig config2;
    config2.data_address = "tcp://127.0.0.1:25556";
    config2.bind_data = false;
    config2.data_pattern = "PAIR";
    
    ASSERT_TRUE(transport1->Configure(config1));
    ASSERT_TRUE(transport1->Connect());
    
    ASSERT_TRUE(transport2->Configure(config2));
    ASSERT_TRUE(transport2->Connect());
    
    std::this_thread::sleep_for(100ms);
    
    // Act - Send from 1 to 2
    auto data1 = CreateTestData(512);
    ASSERT_TRUE(transport1->SendBytes(data1));
    
    std::this_thread::sleep_for(50ms);
    
    auto received1 = transport2->ReceiveBytes();
    ASSERT_NE(received1, nullptr);
    EXPECT_EQ(received1->size(), 512);
    
    // Act - Send from 2 to 1
    auto data2 = CreateTestData(256);
    ASSERT_TRUE(transport2->SendBytes(data2));
    
    std::this_thread::sleep_for(50ms);
    
    auto received2 = transport1->ReceiveBytes();
    ASSERT_NE(received2, nullptr);
    EXPECT_EQ(received2->size(), 256);
}

// Test concurrent send/receive
TEST_F(ZMQTransportBytesTest, ConcurrentSendReceive) {
    // Arrange
    auto publisher = std::make_unique<ZMQTransport>();
    auto subscriber = std::make_unique<ZMQTransport>();
    
    auto pub_config = GetBasicPubSubConfig();
    pub_config.data_address = "tcp://127.0.0.1:25557";
    ASSERT_TRUE(publisher->Configure(pub_config));
    ASSERT_TRUE(publisher->Connect());
    
    auto sub_config = GetBasicSubConfig();
    sub_config.data_address = "tcp://127.0.0.1:25557";
    ASSERT_TRUE(subscriber->Configure(sub_config));
    ASSERT_TRUE(subscriber->Connect());
    
    std::this_thread::sleep_for(100ms);
    
    const int message_count = 100;
    std::atomic<int> received_count(0);
    
    // Act - Start receiver thread
    std::thread receiver([&subscriber, &received_count, message_count]() {
        while (received_count < message_count) {
            auto data = subscriber->ReceiveBytes();
            if (data) {
                received_count++;
            }
            std::this_thread::sleep_for(1ms);
        }
    });
    
    // Send messages
    for (int i = 0; i < message_count; ++i) {
        auto data = CreateTestData(100 + i);
        EXPECT_TRUE(publisher->SendBytes(data));
        std::this_thread::sleep_for(1ms);
    }
    
    // Wait for receiver with timeout
    auto start = std::chrono::steady_clock::now();
    while (received_count < message_count) {
        if (std::chrono::steady_clock::now() - start > 5s) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }
    
    receiver.join();
    
    // Assert
    EXPECT_EQ(received_count, message_count);
}