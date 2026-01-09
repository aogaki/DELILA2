/**
 * @file test_operator_config.cpp
 * @brief Unit tests for OperatorConfig and ComponentAddress structures
 */

#include <delila/core/OperatorConfig.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace DELILA {
namespace test {

class ComponentAddressTest : public ::testing::Test {
protected:
  ComponentAddress address_;
};

TEST_F(ComponentAddressTest, DefaultValues) {
  EXPECT_TRUE(address_.component_id.empty());
  EXPECT_TRUE(address_.command_address.empty());
  EXPECT_TRUE(address_.status_address.empty());
  EXPECT_TRUE(address_.component_type.empty());
  EXPECT_EQ(address_.start_order, 0);
}

TEST_F(ComponentAddressTest, SetComponentId) {
  address_.component_id = "source_01";
  EXPECT_EQ(address_.component_id, "source_01");
}

TEST_F(ComponentAddressTest, SetCommandAddress) {
  address_.command_address = "tcp://192.168.1.10:5555";
  EXPECT_EQ(address_.command_address, "tcp://192.168.1.10:5555");
}

TEST_F(ComponentAddressTest, SetStatusAddress) {
  address_.status_address = "tcp://192.168.1.10:5556";
  EXPECT_EQ(address_.status_address, "tcp://192.168.1.10:5556");
}

TEST_F(ComponentAddressTest, SetComponentType) {
  address_.component_type = "DigitizerSource";
  EXPECT_EQ(address_.component_type, "DigitizerSource");
}

TEST_F(ComponentAddressTest, SetStartOrder) {
  address_.start_order = 1;
  EXPECT_EQ(address_.start_order, 1);
}

class OperatorConfigTest : public ::testing::Test {
protected:
  OperatorConfig config_;
};

TEST_F(OperatorConfigTest, DefaultValues) {
  EXPECT_TRUE(config_.operator_id.empty());
  EXPECT_TRUE(config_.components.empty());
  EXPECT_EQ(config_.configure_timeout_ms, 0);
  EXPECT_EQ(config_.arm_timeout_ms, 0);
  EXPECT_EQ(config_.start_timeout_ms, 0);
  EXPECT_EQ(config_.stop_timeout_ms, 0);
  EXPECT_EQ(config_.command_retry_count, 0);
  EXPECT_EQ(config_.command_retry_interval_ms, 0);
}

TEST_F(OperatorConfigTest, SetOperatorId) {
  config_.operator_id = "operator_01";
  EXPECT_EQ(config_.operator_id, "operator_01");
}

TEST_F(OperatorConfigTest, AddSingleComponent) {
  ComponentAddress addr;
  addr.component_id = "source_01";
  addr.command_address = "tcp://192.168.1.10:5555";
  addr.status_address = "tcp://192.168.1.10:5556";
  addr.component_type = "DigitizerSource";
  addr.start_order = 1;

  config_.components.push_back(addr);

  EXPECT_EQ(config_.components.size(), 1);
  EXPECT_EQ(config_.components[0].component_id, "source_01");
}

TEST_F(OperatorConfigTest, AddMultipleComponents) {
  ComponentAddress source1;
  source1.component_id = "source_01";
  source1.component_type = "DigitizerSource";
  source1.start_order = 1;

  ComponentAddress source2;
  source2.component_id = "source_02";
  source2.component_type = "DigitizerSource";
  source2.start_order = 1;

  ComponentAddress merger;
  merger.component_id = "merger_01";
  merger.component_type = "TimeSortMerger";
  merger.start_order = 2;

  ComponentAddress writer;
  writer.component_id = "writer_01";
  writer.component_type = "FileWriter";
  writer.start_order = 3;

  config_.components.push_back(source1);
  config_.components.push_back(source2);
  config_.components.push_back(merger);
  config_.components.push_back(writer);

  EXPECT_EQ(config_.components.size(), 4);
}

TEST_F(OperatorConfigTest, SetTimeoutSettings) {
  config_.configure_timeout_ms = 10000;
  config_.arm_timeout_ms = 5000;
  config_.start_timeout_ms = 5000;
  config_.stop_timeout_ms = 30000;

  EXPECT_EQ(config_.configure_timeout_ms, 10000);
  EXPECT_EQ(config_.arm_timeout_ms, 5000);
  EXPECT_EQ(config_.start_timeout_ms, 5000);
  EXPECT_EQ(config_.stop_timeout_ms, 30000);
}

TEST_F(OperatorConfigTest, SetRetrySettings) {
  config_.command_retry_count = 3;
  config_.command_retry_interval_ms = 1000;

  EXPECT_EQ(config_.command_retry_count, 3);
  EXPECT_EQ(config_.command_retry_interval_ms, 1000);
}

TEST_F(OperatorConfigTest, ComponentsOrderedByStartOrder) {
  ComponentAddress writer;
  writer.component_id = "writer_01";
  writer.start_order = 3;

  ComponentAddress source;
  source.component_id = "source_01";
  source.start_order = 1;

  ComponentAddress merger;
  merger.component_id = "merger_01";
  merger.start_order = 2;

  config_.components.push_back(writer);
  config_.components.push_back(source);
  config_.components.push_back(merger);

  // Sort by start_order
  std::sort(config_.components.begin(), config_.components.end(),
            [](const ComponentAddress &a, const ComponentAddress &b) {
              return a.start_order < b.start_order;
            });

  EXPECT_EQ(config_.components[0].component_id, "source_01");
  EXPECT_EQ(config_.components[1].component_id, "merger_01");
  EXPECT_EQ(config_.components[2].component_id, "writer_01");
}

TEST_F(OperatorConfigTest, TypicalDAQConfiguration) {
  // Typical DAQ setup: 2 sources -> merger -> writer
  config_.operator_id = "daq_operator";

  ComponentAddress source1;
  source1.component_id = "source_01";
  source1.command_address = "tcp://daq01:5555";
  source1.status_address = "tcp://daq01:5556";
  source1.component_type = "DigitizerSource";
  source1.start_order = 1;

  ComponentAddress source2;
  source2.component_id = "source_02";
  source2.command_address = "tcp://daq02:5555";
  source2.status_address = "tcp://daq02:5556";
  source2.component_type = "DigitizerSource";
  source2.start_order = 1;

  ComponentAddress merger;
  merger.component_id = "merger_01";
  merger.command_address = "tcp://merge:5555";
  merger.status_address = "tcp://merge:5556";
  merger.component_type = "TimeSortMerger";
  merger.start_order = 2;

  ComponentAddress writer;
  writer.component_id = "writer_01";
  writer.command_address = "tcp://storage:5555";
  writer.status_address = "tcp://storage:5556";
  writer.component_type = "FileWriter";
  writer.start_order = 3;

  config_.components.push_back(source1);
  config_.components.push_back(source2);
  config_.components.push_back(merger);
  config_.components.push_back(writer);

  config_.configure_timeout_ms = 10000;
  config_.arm_timeout_ms = 5000;
  config_.start_timeout_ms = 5000;
  config_.stop_timeout_ms = 30000;
  config_.command_retry_count = 3;
  config_.command_retry_interval_ms = 1000;

  EXPECT_EQ(config_.operator_id, "daq_operator");
  EXPECT_EQ(config_.components.size(), 4);
  EXPECT_EQ(config_.configure_timeout_ms, 10000);
}

} // namespace test
} // namespace DELILA
