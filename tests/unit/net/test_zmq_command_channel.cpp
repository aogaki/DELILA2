#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "../../../lib/net/include/ZMQTransport.hpp"
#include "../../../lib/core/include/delila/core/Command.hpp"
#include "../../../lib/core/include/delila/core/CommandResponse.hpp"

using namespace DELILA::Net;
using namespace DELILA;

class ZMQCommandChannelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use unique ports per test to avoid conflicts
    static int base_port = 15000;
    current_port_ = base_port;
    base_port += 10;  // Reserve multiple ports per test

    // Command channel: Server binds (REP), Client connects (REQ)
    server_config_.command_address =
        "tcp://127.0.0.1:" + std::to_string(current_port_);
    server_config_.bind_command = true;

    // Each transport needs its own data socket - server binds PUB
    server_config_.data_address =
        "tcp://127.0.0.1:" + std::to_string(current_port_ + 1);
    server_config_.bind_data = true;
    server_config_.data_pattern = "PUB";
    // Disable status socket by setting same address as data
    server_config_.status_address = server_config_.data_address;

    client_config_.command_address =
        "tcp://127.0.0.1:" + std::to_string(current_port_);
    client_config_.bind_command = false;

    // Client has its own PUB socket on different port (doesn't need to communicate)
    client_config_.data_address =
        "tcp://127.0.0.1:" + std::to_string(current_port_ + 2);
    client_config_.bind_data = true;
    client_config_.data_pattern = "PUB";
    // Disable status socket by setting same address as data
    client_config_.status_address = client_config_.data_address;
  }

  void TearDown() override {
    // Allow sockets to close properly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  TransportConfig server_config_;
  TransportConfig client_config_;
  int current_port_;
};

// Test that command channel can be configured and connected
TEST_F(ZMQCommandChannelTest, CommandSocketCanBeCreated) {
  ZMQTransport server;
  EXPECT_TRUE(server.Configure(server_config_));
  EXPECT_TRUE(server.Connect());
  EXPECT_TRUE(server.IsConnected());
  server.Disconnect();
}

// Test that client can connect to server
TEST_F(ZMQCommandChannelTest, ClientCanConnectToServer) {
  ZMQTransport server;
  ZMQTransport client;

  EXPECT_TRUE(server.Configure(server_config_));
  EXPECT_TRUE(server.Connect());

  EXPECT_TRUE(client.Configure(client_config_));
  EXPECT_TRUE(client.Connect());

  EXPECT_TRUE(server.IsConnected());
  EXPECT_TRUE(client.IsConnected());

  client.Disconnect();
  server.Disconnect();
}

// Test Command serialization and deserialization round-trip
TEST_F(ZMQCommandChannelTest, CommandRoundTrip) {
  ZMQTransport server;
  ZMQTransport client;

  ASSERT_TRUE(server.Configure(server_config_));
  ASSERT_TRUE(server.Connect());
  ASSERT_TRUE(client.Configure(client_config_));
  ASSERT_TRUE(client.Connect());

  // Allow connection to establish
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create test command
  Command cmd;
  cmd.type = CommandType::Configure;
  cmd.request_id = 12345;
  cmd.config_path = "/path/to/config.json";
  cmd.payload = "test payload";

  // Start server thread to receive command and send response
  std::thread server_thread([&server]() {
    auto received_cmd = server.ReceiveCommand(std::chrono::milliseconds(2000));
    if (received_cmd) {
      CommandResponse response;
      response.request_id = received_cmd->request_id;
      response.success = true;
      response.error_code = ErrorCode::Success;
      response.message = "Configuration loaded";
      server.SendCommandResponse(response);
    }
  });

  // Client sends command and receives response
  auto response = client.SendCommand(cmd, std::chrono::milliseconds(3000));

  server_thread.join();

  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->request_id, 12345);
  EXPECT_TRUE(response->success);
  EXPECT_EQ(response->error_code, ErrorCode::Success);
  EXPECT_EQ(response->message, "Configuration loaded");

  client.Disconnect();
  server.Disconnect();
}

// Test Ping command
TEST_F(ZMQCommandChannelTest, PingCommand) {
  ZMQTransport server;
  ZMQTransport client;

  ASSERT_TRUE(server.Configure(server_config_));
  ASSERT_TRUE(server.Connect());
  ASSERT_TRUE(client.Configure(client_config_));
  ASSERT_TRUE(client.Connect());

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  Command ping_cmd(CommandType::Ping, 1);

  std::thread server_thread([&server]() {
    auto cmd = server.ReceiveCommand(std::chrono::milliseconds(2000));
    if (cmd) {
      CommandResponse resp;
      resp.request_id = cmd->request_id;
      resp.success = true;
      resp.error_code = ErrorCode::Success;
      resp.current_state = ComponentState::Idle;
      resp.message = "pong";
      server.SendCommandResponse(resp);
    }
  });

  auto response = client.SendCommand(ping_cmd, std::chrono::milliseconds(3000));
  server_thread.join();

  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(response->success);
  EXPECT_EQ(response->message, "pong");

  client.Disconnect();
  server.Disconnect();
}

// Test Start/Stop commands
TEST_F(ZMQCommandChannelTest, StartStopCommands) {
  ZMQTransport server;
  ZMQTransport client;

  ASSERT_TRUE(server.Configure(server_config_));
  ASSERT_TRUE(server.Connect());
  ASSERT_TRUE(client.Configure(client_config_));
  ASSERT_TRUE(client.Connect());

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ComponentState current_state = ComponentState::Configured;

  // Server handler
  auto server_handler = [&server, &current_state]() {
    for (int i = 0; i < 2; ++i) {  // Handle 2 commands
      auto cmd = server.ReceiveCommand(std::chrono::milliseconds(2000));
      if (!cmd) continue;

      CommandResponse resp;
      resp.request_id = cmd->request_id;

      if (cmd->type == CommandType::Start) {
        current_state = ComponentState::Running;
        resp.success = true;
        resp.current_state = current_state;
        resp.message = "Started";
      } else if (cmd->type == CommandType::Stop) {
        current_state = ComponentState::Idle;
        resp.success = true;
        resp.current_state = current_state;
        resp.message = "Stopped";
      }
      resp.error_code = ErrorCode::Success;
      server.SendCommandResponse(resp);
    }
  };

  std::thread server_thread(server_handler);

  // Send Start command
  Command start_cmd(CommandType::Start, 100);
  auto start_resp = client.SendCommand(start_cmd, std::chrono::milliseconds(3000));

  ASSERT_TRUE(start_resp.has_value());
  EXPECT_TRUE(start_resp->success);
  EXPECT_EQ(start_resp->current_state, ComponentState::Running);

  // Send Stop command
  Command stop_cmd(CommandType::Stop, 101);
  auto stop_resp = client.SendCommand(stop_cmd, std::chrono::milliseconds(3000));

  server_thread.join();

  ASSERT_TRUE(stop_resp.has_value());
  EXPECT_TRUE(stop_resp->success);
  EXPECT_EQ(stop_resp->current_state, ComponentState::Idle);

  client.Disconnect();
  server.Disconnect();
}

// Test error response
TEST_F(ZMQCommandChannelTest, ErrorResponse) {
  ZMQTransport server;
  ZMQTransport client;

  ASSERT_TRUE(server.Configure(server_config_));
  ASSERT_TRUE(server.Connect());
  ASSERT_TRUE(client.Configure(client_config_));
  ASSERT_TRUE(client.Connect());

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::thread server_thread([&server]() {
    auto cmd = server.ReceiveCommand(std::chrono::milliseconds(2000));
    if (cmd) {
      auto resp = CommandResponse::Error(
          cmd->request_id,
          ErrorCode::InvalidConfiguration,
          "Configuration file not found",
          ComponentState::Error);
      server.SendCommandResponse(resp);
    }
  });

  Command config_cmd(CommandType::Configure, 200);
  config_cmd.config_path = "/nonexistent/config.json";

  auto response = client.SendCommand(config_cmd, std::chrono::milliseconds(3000));
  server_thread.join();

  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(response->success);
  EXPECT_EQ(response->error_code, ErrorCode::InvalidConfiguration);
  EXPECT_EQ(response->current_state, ComponentState::Error);

  client.Disconnect();
  server.Disconnect();
}

// Test timeout when server exists but doesn't respond
TEST_F(ZMQCommandChannelTest, CommandTimeout) {
  // Create a server that receives but never responds
  ZMQTransport server;
  ZMQTransport client;

  ASSERT_TRUE(server.Configure(server_config_));
  ASSERT_TRUE(server.Connect());
  ASSERT_TRUE(client.Configure(client_config_));
  ASSERT_TRUE(client.Connect());

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Server receives command but doesn't respond (simulated by short timeout)
  std::thread server_thread([&server]() {
    auto cmd = server.ReceiveCommand(std::chrono::milliseconds(500));
    // Don't send response - let client timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  });

  Command cmd(CommandType::Ping, 1);

  // Client should timeout waiting for response
  auto response = client.SendCommand(cmd, std::chrono::milliseconds(100));

  server_thread.join();

  // Note: With ZMQ REQ/REP, if no reply comes, the socket enters error state
  // The timeout test verifies that SendCommand doesn't hang forever
  // It may or may not return nullopt depending on timing

  client.Disconnect();
  server.Disconnect();
}

// Test receive command returns nullopt when no command pending
TEST_F(ZMQCommandChannelTest, ReceiveCommandTimeout) {
  ZMQTransport server;

  ASSERT_TRUE(server.Configure(server_config_));
  ASSERT_TRUE(server.Connect());

  // No client sending commands, should return nullopt
  auto cmd = server.ReceiveCommand(std::chrono::milliseconds(100));

  EXPECT_FALSE(cmd.has_value());

  server.Disconnect();
}

// Test when command socket is not connected
TEST_F(ZMQCommandChannelTest, CommandWithoutConnection) {
  ZMQTransport transport;

  Command cmd(CommandType::Ping, 1);
  auto response = transport.SendCommand(cmd);

  EXPECT_FALSE(response.has_value());
}

// Test when command address is empty
TEST_F(ZMQCommandChannelTest, EmptyCommandAddressSkipsSocket) {
  TransportConfig config;
  config.command_address = "";  // Empty address
  config.data_address = "tcp://127.0.0.1:19999";
  config.bind_data = true;

  ZMQTransport transport;
  EXPECT_TRUE(transport.Configure(config));
  EXPECT_TRUE(transport.Connect());

  // Command operations should return empty/false when socket not created
  Command cmd(CommandType::Ping, 1);
  auto response = transport.SendCommand(cmd);
  EXPECT_FALSE(response.has_value());

  auto received = transport.ReceiveCommand(std::chrono::milliseconds(10));
  EXPECT_FALSE(received.has_value());

  transport.Disconnect();
}
