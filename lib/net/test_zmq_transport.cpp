#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

class ZMQTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Minimal setup following KISS principle
        transport = std::make_unique<ZMQTransport>();
    }
    
    // Helper function to create test EventData
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEvents(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            event->timeStampNs = i * 1000;
            event->energy = i * 100;
            event->module = i % 4;
            event->channel = i % 8;
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    std::unique_ptr<ZMQTransport> transport;
};

// GREEN: First test - basic construction (passes)
TEST_F(ZMQTransportTest, ConstructorCreatesValidInstance) {
    EXPECT_FALSE(transport->IsConnected());
}

// RED: Second failing test - configuration validation
TEST_F(ZMQTransportTest, RejectsInvalidConfiguration) {
    TransportConfig invalid_config;  // Empty config should be invalid
    invalid_config.data_address = "";  // Invalid empty address
    EXPECT_FALSE(transport->Configure(invalid_config));
}

// GREEN: Third test - accepts valid configuration (passes)
TEST_F(ZMQTransportTest, AcceptsValidConfiguration) {
    TransportConfig valid_config;
    valid_config.data_address = "tcp://localhost:5555";
    EXPECT_TRUE(transport->Configure(valid_config));
}

// RED: Fourth failing test - connection requires configuration
TEST_F(ZMQTransportTest, ConnectFailsWithoutConfiguration) {
    EXPECT_FALSE(transport->Connect());
    EXPECT_FALSE(transport->IsConnected());
}

// GREEN: Fifth test - connection works with valid configuration (passes)
TEST_F(ZMQTransportTest, ConnectSucceedsWithValidConfiguration) {
    TransportConfig valid_config;
    valid_config.data_address = "tcp://localhost:5555";
    transport->Configure(valid_config);
    
    EXPECT_TRUE(transport->Connect());
    EXPECT_TRUE(transport->IsConnected());
}

// RED: Sixth failing test - Send fails when not connected
TEST_F(ZMQTransportTest, SendFailsWhenNotConnected) {
    auto events = CreateTestEvents(1);
    EXPECT_FALSE(transport->Send(events));
}

// RED: Seventh failing test - Send works when connected
TEST_F(ZMQTransportTest, SendWorksWhenConnected) {
    TransportConfig valid_config;
    valid_config.data_address = "tcp://localhost:5556";  // Different port to avoid conflicts
    transport->Configure(valid_config);
    transport->Connect();
    
    auto events = CreateTestEvents(2);
    EXPECT_TRUE(transport->Send(events));
}

// RED: Test receive functionality
TEST_F(ZMQTransportTest, ReceiveFailsWhenNotConnected) {
    auto result = transport->Receive();
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

// TODO: Fix PUB/SUB timing issues in integration test
TEST_F(ZMQTransportTest, DISABLED_ReceiveWorksWithPubSubPattern) {
    // Set up publisher (sender) - binds to send data
    TransportConfig pub_config;
    pub_config.data_address = "tcp://localhost:5558";
    pub_config.bind_data = true;
    pub_config.is_publisher = true;  // Explicitly set as publisher
    auto publisher = std::make_unique<ZMQTransport>();
    publisher->Configure(pub_config);
    publisher->Connect();
    
    // Set up subscriber (receiver) - connects to receive data  
    TransportConfig sub_config;
    sub_config.data_address = "tcp://localhost:5558";  // Same address
    sub_config.bind_data = false;  // Subscriber connects
    sub_config.is_publisher = false;  // Explicitly set as subscriber
    transport->Configure(sub_config);
    transport->Connect();
    
    // Allow time for ZMQ connection setup (PUB/SUB slow joiner problem)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Send test data multiple times to account for slow joiner
    auto events = CreateTestEvents(2);
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(publisher->Send(events));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Try to receive data (retry multiple times due to timing)
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> received_events = nullptr;
    uint64_t sequence = 0;
    
    for (int retry = 0; retry < 5; ++retry) {
        auto result = transport->Receive();
        if (result.first) {
            received_events = std::move(result.first);
            sequence = result.second;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    EXPECT_NE(received_events, nullptr);
    if (received_events) {
        EXPECT_EQ(received_events->size(), 2);
    }
}

// RED: Test status functionality
TEST_F(ZMQTransportTest, SendStatusFailsWhenNotConnected) {
    ComponentStatus status;
    status.component_id = "test_component";
    status.state = "idle";
    EXPECT_FALSE(transport->SendStatus(status));
}

TEST_F(ZMQTransportTest, ReceiveStatusFailsWhenNotConnected) {
    auto status = transport->ReceiveStatus();
    EXPECT_EQ(status, nullptr);
}

// RED: Test command functionality
TEST_F(ZMQTransportTest, SendCommandFailsWhenNotConnected) {
    Command cmd;
    cmd.command_id = "test_cmd_001";
    cmd.type = CommandType::START;
    cmd.target_component = "digitizer";
    
    auto response = transport->SendCommand(cmd);
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.command_id, cmd.command_id);
}

TEST_F(ZMQTransportTest, ReceiveCommandFailsWhenNotConnected) {
    auto command = transport->ReceiveCommand();
    EXPECT_EQ(command, nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}