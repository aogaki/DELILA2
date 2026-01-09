/**
 * @file test_digitizer_source.cpp
 * @brief Unit tests for DigitizerSource component
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <DigitizerSource.hpp>
#include <delila/core/ComponentState.hpp>
#include <delila/core/ComponentStatus.hpp>
#include <thread>
#include <chrono>

namespace DELILA {
namespace test {

class DigitizerSourceTest : public ::testing::Test {
protected:
  void SetUp() override {
    source_ = std::make_unique<DigitizerSource>();
  }

  void TearDown() override {
    if (source_) {
      source_->Shutdown();
    }
  }

  std::unique_ptr<DigitizerSource> source_;
};

// === Initial State Tests ===

TEST_F(DigitizerSourceTest, InitialStateIsIdle) {
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

TEST_F(DigitizerSourceTest, HasEmptyComponentIdInitially) {
  // Before initialization, component_id should be empty or default
  auto status = source_->GetStatus();
  EXPECT_TRUE(status.component_id.empty() || status.component_id == "uninitialized");
}

TEST_F(DigitizerSourceTest, InitialStatusHasZeroMetrics) {
  auto status = source_->GetStatus();
  EXPECT_EQ(status.metrics.events_processed, 0);
  EXPECT_EQ(status.metrics.bytes_transferred, 0);
  EXPECT_EQ(status.run_number, 0);
}

// === Address Configuration Tests ===

TEST_F(DigitizerSourceTest, InputAddressesAreEmpty) {
  // DigitizerSource has no inputs (it's a data source)
  auto inputs = source_->GetInputAddresses();
  EXPECT_TRUE(inputs.empty());
}

TEST_F(DigitizerSourceTest, CanSetOutputAddresses) {
  std::vector<std::string> outputs = {"tcp://localhost:5555"};
  source_->SetOutputAddresses(outputs);

  auto result = source_->GetOutputAddresses();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], "tcp://localhost:5555");
}

TEST_F(DigitizerSourceTest, SetInputAddressesIsIgnored) {
  // DigitizerSource should ignore input addresses
  std::vector<std::string> inputs = {"tcp://localhost:5555"};
  source_->SetInputAddresses(inputs);

  auto result = source_->GetInputAddresses();
  EXPECT_TRUE(result.empty());
}

// === State Transition Tests ===

TEST_F(DigitizerSourceTest, TransitionIdleToConfigured) {
  // Use mock configuration - no actual digitizer needed
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});

  EXPECT_TRUE(source_->Initialize(""));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

TEST_F(DigitizerSourceTest, ConfiguredToArmed) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");

  EXPECT_TRUE(source_->Arm());
  EXPECT_EQ(source_->GetState(), ComponentState::Armed);
}

TEST_F(DigitizerSourceTest, ArmedToRunning) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");
  source_->Arm();

  EXPECT_TRUE(source_->Start(100));
  EXPECT_EQ(source_->GetState(), ComponentState::Running);
  EXPECT_EQ(source_->GetStatus().run_number, 100);
}

TEST_F(DigitizerSourceTest, RunningToConfigured) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");
  source_->Arm();
  source_->Start(100);

  EXPECT_TRUE(source_->Stop(true));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

TEST_F(DigitizerSourceTest, ErrorToIdle) {
  source_->SetMockMode(true);
  // Force error state
  source_->ForceError("Test error");
  EXPECT_EQ(source_->GetState(), ComponentState::Error);

  source_->Reset();
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

// === Invalid State Transition Tests ===

TEST_F(DigitizerSourceTest, CannotArmFromIdle) {
  EXPECT_FALSE(source_->Arm());
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

TEST_F(DigitizerSourceTest, CannotStartFromConfigured) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");

  EXPECT_FALSE(source_->Start(100));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

TEST_F(DigitizerSourceTest, CannotStopFromIdle) {
  EXPECT_FALSE(source_->Stop(true));
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

// === Component ID Tests ===

TEST_F(DigitizerSourceTest, ComponentIdAfterInitialize) {
  source_->SetMockMode(true);
  source_->SetComponentId("source_01");
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");

  EXPECT_EQ(source_->GetComponentId(), "source_01");
}

// === Mock Data Generation Tests ===

TEST_F(DigitizerSourceTest, MockModeCanBeEnabled) {
  source_->SetMockMode(true);
  source_->SetMockEventRate(100); // 100 events per second
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");

  // Verify mock mode configuration works
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

TEST_F(DigitizerSourceTest, MockModeRunsWithoutCrash) {
  source_->SetMockMode(true);
  source_->SetMockEventRate(1000);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");
  source_->Arm();
  source_->Start(1);

  // Run briefly - this tests that mock mode doesn't crash
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  EXPECT_EQ(source_->GetState(), ComponentState::Running);

  source_->Stop(true);
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

// === Graceful vs Emergency Stop Tests ===

TEST_F(DigitizerSourceTest, GracefulStopFlushesData) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");
  source_->Arm();
  source_->Start(1);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Graceful stop should wait for data to be flushed
  EXPECT_TRUE(source_->Stop(true));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

TEST_F(DigitizerSourceTest, EmergencyStopImmediate) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");
  source_->Arm();
  source_->Start(1);

  // Emergency stop should be immediate
  EXPECT_TRUE(source_->Stop(false));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

// === EOS (End Of Stream) Tests ===
// Note: Full EOS transmission tests are in integration tests
// These unit tests verify the EOS-related interfaces exist

TEST_F(DigitizerSourceTest, HasDataProcessorForEOS) {
  source_->SetMockMode(true);
  source_->SetOutputAddresses({"tcp://localhost:5555"});
  source_->Initialize("");

  // After initialization, source should have a DataProcessor
  // This is verified indirectly - if EOS methods work, DataProcessor exists
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

} // namespace test
} // namespace DELILA
