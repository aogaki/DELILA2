/**
 * @file test_component_config.cpp
 * @brief Unit tests for ComponentConfig and TransportConfig structures
 */

#include <delila/core/ComponentConfig.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace DELILA {
namespace test {

class TransportConfigTest : public ::testing::Test {
protected:
  TransportConfig config_;
};

TEST_F(TransportConfigTest, DefaultValues) {
  EXPECT_TRUE(config_.data_address.empty());
  EXPECT_TRUE(config_.status_address.empty());
  EXPECT_TRUE(config_.command_address.empty());
  EXPECT_FALSE(config_.bind_data);
  EXPECT_FALSE(config_.bind_status);
  EXPECT_FALSE(config_.bind_command);
  EXPECT_TRUE(config_.data_pattern.empty());
}

TEST_F(TransportConfigTest, SetDataAddress) {
  config_.data_address = "tcp://localhost:5555";
  EXPECT_EQ(config_.data_address, "tcp://localhost:5555");
}

TEST_F(TransportConfigTest, SetStatusAddress) {
  config_.status_address = "tcp://localhost:5556";
  EXPECT_EQ(config_.status_address, "tcp://localhost:5556");
}

TEST_F(TransportConfigTest, SetCommandAddress) {
  config_.command_address = "tcp://localhost:5557";
  EXPECT_EQ(config_.command_address, "tcp://localhost:5557");
}

TEST_F(TransportConfigTest, SetBindFlags) {
  config_.bind_data = true;
  config_.bind_status = true;
  config_.bind_command = false;

  EXPECT_TRUE(config_.bind_data);
  EXPECT_TRUE(config_.bind_status);
  EXPECT_FALSE(config_.bind_command);
}

TEST_F(TransportConfigTest, SetDataPattern) {
  config_.data_pattern = "PUSH/PULL";
  EXPECT_EQ(config_.data_pattern, "PUSH/PULL");
}

class ComponentConfigTest : public ::testing::Test {
protected:
  ComponentConfig config_;
};

TEST_F(ComponentConfigTest, DefaultValues) {
  EXPECT_TRUE(config_.component_id.empty());
  EXPECT_TRUE(config_.component_type.empty());
  EXPECT_TRUE(config_.input_addresses.empty());
  EXPECT_TRUE(config_.output_addresses.empty());
  EXPECT_EQ(config_.queue_max_size, 0);
  EXPECT_EQ(config_.queue_warning_threshold, 0);
  EXPECT_EQ(config_.status_interval_ms, 0);
  EXPECT_EQ(config_.command_timeout_ms, 0);
}

TEST_F(ComponentConfigTest, SetComponentId) {
  config_.component_id = "source_01";
  EXPECT_EQ(config_.component_id, "source_01");
}

TEST_F(ComponentConfigTest, SetComponentType) {
  config_.component_type = "DigitizerSource";
  EXPECT_EQ(config_.component_type, "DigitizerSource");
}

TEST_F(ComponentConfigTest, SetInputAddresses) {
  config_.input_addresses = {"tcp://localhost:5555", "tcp://localhost:5556"};
  EXPECT_EQ(config_.input_addresses.size(), 2);
  EXPECT_EQ(config_.input_addresses[0], "tcp://localhost:5555");
}

TEST_F(ComponentConfigTest, SetOutputAddresses) {
  config_.output_addresses = {"tcp://localhost:6666"};
  EXPECT_EQ(config_.output_addresses.size(), 1);
  EXPECT_EQ(config_.output_addresses[0], "tcp://localhost:6666");
}

TEST_F(ComponentConfigTest, SetQueueSettings) {
  config_.queue_max_size = 10000;
  config_.queue_warning_threshold = 8000;

  EXPECT_EQ(config_.queue_max_size, 10000);
  EXPECT_EQ(config_.queue_warning_threshold, 8000);
}

TEST_F(ComponentConfigTest, SetTimingSettings) {
  config_.status_interval_ms = 1000;
  config_.command_timeout_ms = 5000;

  EXPECT_EQ(config_.status_interval_ms, 1000);
  EXPECT_EQ(config_.command_timeout_ms, 5000);
}

TEST_F(ComponentConfigTest, HasEmbeddedTransportConfig) {
  config_.transport.data_address = "tcp://localhost:5555";
  config_.transport.bind_data = true;

  EXPECT_EQ(config_.transport.data_address, "tcp://localhost:5555");
  EXPECT_TRUE(config_.transport.bind_data);
}

TEST_F(ComponentConfigTest, SourceConfigPattern) {
  // DigitizerSource: 0 inputs, 1 output
  config_.component_id = "source_01";
  config_.component_type = "DigitizerSource";
  config_.input_addresses = {};
  config_.output_addresses = {"tcp://localhost:5555"};
  config_.transport.data_address = "tcp://*:5555";
  config_.transport.bind_data = true;
  config_.transport.data_pattern = "PUSH";

  EXPECT_TRUE(config_.input_addresses.empty());
  EXPECT_EQ(config_.output_addresses.size(), 1);
  EXPECT_TRUE(config_.transport.bind_data);
}

TEST_F(ComponentConfigTest, SinkConfigPattern) {
  // FileWriter: 1 input, 0 outputs
  config_.component_id = "writer_01";
  config_.component_type = "FileWriter";
  config_.input_addresses = {"tcp://localhost:5555"};
  config_.output_addresses = {};
  config_.transport.data_address = "tcp://localhost:5555";
  config_.transport.bind_data = false;
  config_.transport.data_pattern = "PULL";

  EXPECT_EQ(config_.input_addresses.size(), 1);
  EXPECT_TRUE(config_.output_addresses.empty());
  EXPECT_FALSE(config_.transport.bind_data);
}

TEST_F(ComponentConfigTest, MergerConfigPattern) {
  // TimeSortMerger: N inputs, 1 output
  config_.component_id = "merger_01";
  config_.component_type = "TimeSortMerger";
  config_.input_addresses = {"tcp://host1:5555", "tcp://host2:5555",
                             "tcp://host3:5555"};
  config_.output_addresses = {"tcp://localhost:6666"};

  EXPECT_EQ(config_.input_addresses.size(), 3);
  EXPECT_EQ(config_.output_addresses.size(), 1);
}

} // namespace test
} // namespace DELILA
