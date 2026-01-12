/**
 * @file test_emulator.cpp
 * @brief Unit tests for Emulator component
 *
 * TDD: Tests written first, then implementation.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "Emulator.hpp"
#include "delila/core/ComponentState.hpp"

using namespace DELILA;
using namespace std::chrono_literals;

// Port manager to avoid conflicts between tests
class EmulatorPortManager {
 private:
  static std::atomic<int> next_port_;

 public:
  static int GetNextPort() { return next_port_.fetch_add(1); }
};

std::atomic<int> EmulatorPortManager::next_port_{40000};

class EmulatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    emulator_ = std::make_unique<Emulator>();
    port_ = EmulatorPortManager::GetNextPort();
    address_ = "tcp://127.0.0.1:" + std::to_string(port_);
  }

  void TearDown() override {
    if (emulator_) {
      emulator_->Shutdown();
      emulator_.reset();
    }
    // Wait for socket cleanup
    std::this_thread::sleep_for(50ms);
  }

  std::unique_ptr<Emulator> emulator_;
  int port_;
  std::string address_;
};

// === Initial State Tests ===

TEST_F(EmulatorTest, InitialStateIsIdle) {
  EXPECT_EQ(emulator_->GetState(), ComponentState::Idle);
}

TEST_F(EmulatorTest, HasEmptyComponentIdInitially) {
  EXPECT_TRUE(emulator_->GetComponentId().empty());
}

TEST_F(EmulatorTest, InitialStatusHasZeroMetrics) {
  auto status = emulator_->GetStatus();
  EXPECT_EQ(status.metrics.events_processed, 0);
  EXPECT_EQ(status.metrics.bytes_transferred, 0);
}

// === Configuration Tests ===

TEST_F(EmulatorTest, CanSetModuleNumber) {
  emulator_->SetModuleNumber(5);
  EXPECT_EQ(emulator_->GetModuleNumber(), 5);
}

TEST_F(EmulatorTest, CanSetNumChannels) {
  emulator_->SetNumChannels(32);
  EXPECT_EQ(emulator_->GetNumChannels(), 32);
}

TEST_F(EmulatorTest, CanSetEventRate) {
  emulator_->SetEventRate(10000);
  EXPECT_EQ(emulator_->GetEventRate(), 10000);
}

TEST_F(EmulatorTest, CanSetDataMode) {
  emulator_->SetDataMode(EmulatorDataMode::Full);
  EXPECT_EQ(emulator_->GetDataMode(), EmulatorDataMode::Full);
}

TEST_F(EmulatorTest, DefaultDataModeIsMinimal) {
  EXPECT_EQ(emulator_->GetDataMode(), EmulatorDataMode::Minimal);
}

TEST_F(EmulatorTest, CanSetEnergyRange) {
  emulator_->SetEnergyRange(100, 8000);
  auto [min, max] = emulator_->GetEnergyRange();
  EXPECT_EQ(min, 100);
  EXPECT_EQ(max, 8000);
}

TEST_F(EmulatorTest, CanSetOutputAddress) {
  emulator_->SetOutputAddresses({address_});
  auto addresses = emulator_->GetOutputAddresses();
  ASSERT_EQ(addresses.size(), 1);
  EXPECT_EQ(addresses[0], address_);
}

TEST_F(EmulatorTest, CanSetComponentId) {
  emulator_->SetComponentId("emu_test");
  EXPECT_EQ(emulator_->GetComponentId(), "emu_test");
}

// === State Transition Tests ===

TEST_F(EmulatorTest, TransitionIdleToConfigured) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});

  EXPECT_TRUE(emulator_->Initialize(""));
  EXPECT_EQ(emulator_->GetState(), ComponentState::Configured);
}

TEST_F(EmulatorTest, InitializeFailsWithoutOutputAddress) {
  emulator_->SetModuleNumber(0);
  // No output address set

  EXPECT_FALSE(emulator_->Initialize(""));
  EXPECT_EQ(emulator_->GetState(), ComponentState::Idle);
}

TEST_F(EmulatorTest, ConfiguredToArmed) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");

  EXPECT_TRUE(emulator_->Arm());
  EXPECT_EQ(emulator_->GetState(), ComponentState::Armed);
}

TEST_F(EmulatorTest, ArmedToRunning) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");
  emulator_->Arm();

  EXPECT_TRUE(emulator_->Start(1));
  EXPECT_EQ(emulator_->GetState(), ComponentState::Running);
}

TEST_F(EmulatorTest, RunningToConfigured) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");
  emulator_->Arm();
  emulator_->Start(1);

  EXPECT_TRUE(emulator_->Stop(true));
  EXPECT_EQ(emulator_->GetState(), ComponentState::Configured);
}

TEST_F(EmulatorTest, ErrorToIdle) {
  emulator_->ForceError("Test error");
  EXPECT_EQ(emulator_->GetState(), ComponentState::Error);

  emulator_->Reset();
  EXPECT_EQ(emulator_->GetState(), ComponentState::Idle);
}

// === Invalid Transition Tests ===

TEST_F(EmulatorTest, CannotArmFromIdle) {
  EXPECT_FALSE(emulator_->Arm());
  EXPECT_EQ(emulator_->GetState(), ComponentState::Idle);
}

TEST_F(EmulatorTest, CannotStartFromConfigured) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");

  EXPECT_FALSE(emulator_->Start(1));
  EXPECT_EQ(emulator_->GetState(), ComponentState::Configured);
}

TEST_F(EmulatorTest, CannotStopFromIdle) {
  EXPECT_FALSE(emulator_->Stop(true));
  EXPECT_EQ(emulator_->GetState(), ComponentState::Idle);
}

// === Module Number Tests ===

TEST_F(EmulatorTest, ModuleNumberIsFixedDuringRun) {
  emulator_->SetModuleNumber(7);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");
  emulator_->Arm();
  emulator_->Start(1);

  // Module number should remain constant
  EXPECT_EQ(emulator_->GetModuleNumber(), 7);

  emulator_->Stop(true);
}

// === Run Number Tests ===

TEST_F(EmulatorTest, RunNumberSetOnStart) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");
  emulator_->Arm();
  emulator_->Start(42);

  auto status = emulator_->GetStatus();
  EXPECT_EQ(status.run_number, 42);

  emulator_->Stop(true);
}

// === Multiple Run Tests ===

TEST_F(EmulatorTest, MultipleRunCycles) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");

  for (uint32_t run = 1; run <= 3; ++run) {
    EXPECT_TRUE(emulator_->Arm()) << "Failed to arm for run " << run;
    EXPECT_TRUE(emulator_->Start(run)) << "Failed to start run " << run;

    auto status = emulator_->GetStatus();
    EXPECT_EQ(status.run_number, run);

    EXPECT_TRUE(emulator_->Stop(true)) << "Failed to stop run " << run;
  }
}

// === Graceful vs Emergency Stop ===

TEST_F(EmulatorTest, GracefulStopWaitsForProcessing) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->SetEventRate(1000);
  emulator_->Initialize("");
  emulator_->Arm();
  emulator_->Start(1);

  std::this_thread::sleep_for(100ms);

  auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(emulator_->Stop(true));
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_EQ(emulator_->GetState(), ComponentState::Configured);
}

TEST_F(EmulatorTest, EmergencyStopIsImmediate) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->SetEventRate(1000);
  emulator_->Initialize("");
  emulator_->Arm();
  emulator_->Start(1);

  std::this_thread::sleep_for(50ms);

  auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(emulator_->Stop(false));
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_EQ(emulator_->GetState(), ComponentState::Configured);
}

// === Status Tests ===

TEST_F(EmulatorTest, StatusContainsComponentId) {
  emulator_->SetComponentId("test_emulator");
  auto status = emulator_->GetStatus();
  EXPECT_EQ(status.component_id, "test_emulator");
}

TEST_F(EmulatorTest, StatusContainsState) {
  emulator_->SetModuleNumber(0);
  emulator_->SetOutputAddresses({address_});
  emulator_->Initialize("");

  auto status = emulator_->GetStatus();
  EXPECT_EQ(status.state, ComponentState::Configured);
}

// === Data Mode Tests ===

TEST_F(EmulatorTest, MinimalModeIsDefault) {
  EXPECT_EQ(emulator_->GetDataMode(), EmulatorDataMode::Minimal);
}

TEST_F(EmulatorTest, CanSwitchToFullMode) {
  emulator_->SetDataMode(EmulatorDataMode::Full);
  EXPECT_EQ(emulator_->GetDataMode(), EmulatorDataMode::Full);
}

TEST_F(EmulatorTest, WaveformSizeOnlyUsedInFullMode) {
  emulator_->SetWaveformSize(1024);
  EXPECT_EQ(emulator_->GetWaveformSize(), 1024);
}

// === Default Values Tests ===

TEST_F(EmulatorTest, DefaultNumChannelsIs16) {
  EXPECT_EQ(emulator_->GetNumChannels(), 16);
}

TEST_F(EmulatorTest, DefaultEventRateIs1000) {
  EXPECT_EQ(emulator_->GetEventRate(), 1000);
}

TEST_F(EmulatorTest, DefaultEnergyRangeIs0To16383) {
  auto [min, max] = emulator_->GetEnergyRange();
  EXPECT_EQ(min, 0);
  EXPECT_EQ(max, 16383);
}
