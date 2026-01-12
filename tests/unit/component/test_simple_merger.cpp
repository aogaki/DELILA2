/**
 * @file test_simple_merger.cpp
 * @brief Unit tests for SimpleMerger component
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "SimpleMerger.hpp"
#include "delila/core/ComponentState.hpp"
#include "delila/core/ComponentStatus.hpp"

namespace DELILA {
namespace test {

class SimpleMergerTest : public ::testing::Test {
 protected:
  void SetUp() override { merger_ = std::make_unique<SimpleMerger>(); }

  void TearDown() override {
    if (merger_) {
      merger_->Shutdown();
    }
  }

  std::unique_ptr<SimpleMerger> merger_;
};

// === Initial State Tests ===

TEST_F(SimpleMergerTest, InitialStateIsIdle) {
  EXPECT_EQ(merger_->GetState(), ComponentState::Idle);
}

TEST_F(SimpleMergerTest, HasEmptyComponentIdInitially) {
  auto status = merger_->GetStatus();
  EXPECT_TRUE(status.component_id.empty() ||
              status.component_id == "uninitialized");
}

TEST_F(SimpleMergerTest, InitialStatusHasZeroMetrics) {
  auto status = merger_->GetStatus();
  EXPECT_EQ(status.metrics.events_processed, 0);
  EXPECT_EQ(status.metrics.bytes_transferred, 0);
  EXPECT_EQ(status.run_number, 0);
}

TEST_F(SimpleMergerTest, InitialQueueIsEmpty) {
  EXPECT_EQ(merger_->GetQueueSize(), 0);
}

// === Address Configuration Tests ===

TEST_F(SimpleMergerTest, CanSetMultipleInputAddresses) {
  std::vector<std::string> inputs = {"tcp://localhost:5555",
                                     "tcp://localhost:5556",
                                     "tcp://localhost:5557"};
  merger_->SetInputAddresses(inputs);

  auto result = merger_->GetInputAddresses();
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], "tcp://localhost:5555");
  EXPECT_EQ(result[1], "tcp://localhost:5556");
  EXPECT_EQ(result[2], "tcp://localhost:5557");
}

TEST_F(SimpleMergerTest, CanSetOutputAddress) {
  std::vector<std::string> outputs = {"tcp://localhost:6666"};
  merger_->SetOutputAddresses(outputs);

  auto result = merger_->GetOutputAddresses();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], "tcp://localhost:6666");
}

TEST_F(SimpleMergerTest, InputCountMatchesConfiguredInputs) {
  std::vector<std::string> inputs = {"tcp://localhost:5555",
                                     "tcp://localhost:5556"};
  merger_->SetInputAddresses(inputs);

  EXPECT_EQ(merger_->GetInputCount(), 2);
}

// === State Transition Tests ===

TEST_F(SimpleMergerTest, TransitionIdleToConfigured) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});

  EXPECT_TRUE(merger_->Initialize(""));
  EXPECT_EQ(merger_->GetState(), ComponentState::Configured);
}

TEST_F(SimpleMergerTest, ConfiguredToArmed) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");

  EXPECT_TRUE(merger_->Arm());
  EXPECT_EQ(merger_->GetState(), ComponentState::Armed);
}

TEST_F(SimpleMergerTest, ArmedToRunning) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");
  merger_->Arm();

  EXPECT_TRUE(merger_->Start(100));
  EXPECT_EQ(merger_->GetState(), ComponentState::Running);
  EXPECT_EQ(merger_->GetStatus().run_number, 100);
}

TEST_F(SimpleMergerTest, RunningToConfigured) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");
  merger_->Arm();
  merger_->Start(100);

  EXPECT_TRUE(merger_->Stop(true));
  EXPECT_EQ(merger_->GetState(), ComponentState::Configured);
}

TEST_F(SimpleMergerTest, ErrorToIdle) {
  // Force error state
  merger_->ForceError("Test error");
  EXPECT_EQ(merger_->GetState(), ComponentState::Error);

  merger_->Reset();
  EXPECT_EQ(merger_->GetState(), ComponentState::Idle);
}

// === Invalid State Transition Tests ===

TEST_F(SimpleMergerTest, CannotArmFromIdle) {
  EXPECT_FALSE(merger_->Arm());
  EXPECT_EQ(merger_->GetState(), ComponentState::Idle);
}

TEST_F(SimpleMergerTest, CannotStartFromConfigured) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");

  EXPECT_FALSE(merger_->Start(100));
  EXPECT_EQ(merger_->GetState(), ComponentState::Configured);
}

TEST_F(SimpleMergerTest, CannotStopFromIdle) {
  EXPECT_FALSE(merger_->Stop(true));
  EXPECT_EQ(merger_->GetState(), ComponentState::Idle);
}

// === Component ID Tests ===

TEST_F(SimpleMergerTest, ComponentIdAfterInitialize) {
  merger_->SetComponentId("merger_01");
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");

  EXPECT_EQ(merger_->GetComponentId(), "merger_01");
}

// === Validation Tests ===

TEST_F(SimpleMergerTest, InitializeFailsWithNoInputs) {
  merger_->SetOutputAddresses({"tcp://localhost:6666"});

  EXPECT_FALSE(merger_->Initialize(""));
  EXPECT_EQ(merger_->GetState(), ComponentState::Idle);
}

TEST_F(SimpleMergerTest, InitializeFailsWithNoOutputs) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});

  EXPECT_FALSE(merger_->Initialize(""));
  EXPECT_EQ(merger_->GetState(), ComponentState::Idle);
}

// === Graceful vs Emergency Stop Tests ===

TEST_F(SimpleMergerTest, GracefulStopWaitsForProcessing) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");
  merger_->Arm();
  merger_->Start(1);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Graceful stop should wait for pending data
  EXPECT_TRUE(merger_->Stop(true));
  EXPECT_EQ(merger_->GetState(), ComponentState::Configured);
}

TEST_F(SimpleMergerTest, EmergencyStopIsImmediate) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");
  merger_->Arm();
  merger_->Start(1);

  // Emergency stop should be immediate
  EXPECT_TRUE(merger_->Stop(false));
  EXPECT_EQ(merger_->GetState(), ComponentState::Configured);
}

// === Multiple Input Source Tests ===

TEST_F(SimpleMergerTest, TracksMultipleSources) {
  std::vector<std::string> inputs = {"tcp://localhost:5555",
                                     "tcp://localhost:5556",
                                     "tcp://localhost:5557"};
  merger_->SetInputAddresses(inputs);
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");

  // Merger should track all 3 sources
  EXPECT_EQ(merger_->GetInputCount(), 3);
}

// === Run Lifecycle Tests ===

TEST_F(SimpleMergerTest, MultipleRunCycles) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");

  // First run
  merger_->Arm();
  merger_->Start(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  merger_->Stop(true);

  // Should be able to do another run
  merger_->Arm();
  merger_->Start(2);
  EXPECT_EQ(merger_->GetStatus().run_number, 2);
  merger_->Stop(true);
}

// === Queue Status Tests ===

TEST_F(SimpleMergerTest, QueueSizeReportedInStatus) {
  merger_->SetInputAddresses({"tcp://localhost:5555"});
  merger_->SetOutputAddresses({"tcp://localhost:6666"});
  merger_->Initialize("");

  auto status = merger_->GetStatus();
  EXPECT_EQ(status.metrics.queue_size, 0);
  EXPECT_GT(status.metrics.queue_max, 0);  // Should have a max queue size
}

}  // namespace test
}  // namespace DELILA
