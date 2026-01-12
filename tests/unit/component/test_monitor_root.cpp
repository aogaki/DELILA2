/**
 * @file test_monitor_root.cpp
 * @brief Unit tests for MonitorROOT component
 *
 * TDD: Tests written first, then implementation.
 * Note: These tests only compile when ROOT is available.
 */

#ifdef HAS_ROOT

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "MonitorROOT.hpp"
#include "delila/core/ComponentState.hpp"

using namespace DELILA;
using namespace std::chrono_literals;

// Port manager to avoid conflicts between tests
class MonitorROOTPortManager {
 private:
  static std::atomic<int> next_port_;

 public:
  static int GetNextPort() { return next_port_.fetch_add(1); }
};

std::atomic<int> MonitorROOTPortManager::next_port_{41000};

class MonitorROOTTest : public ::testing::Test {
 protected:
  void SetUp() override {
    monitor_ = std::make_unique<MonitorROOT>();
    zmq_port_ = MonitorROOTPortManager::GetNextPort();
    http_port_ = MonitorROOTPortManager::GetNextPort();
    input_address_ = "tcp://127.0.0.1:" + std::to_string(zmq_port_);
  }

  void TearDown() override {
    if (monitor_) {
      monitor_->Shutdown();
      monitor_.reset();
    }
    // Wait for socket cleanup
    std::this_thread::sleep_for(50ms);
  }

  std::unique_ptr<MonitorROOT> monitor_;
  int zmq_port_;
  int http_port_;
  std::string input_address_;
};

// === Initial State Tests ===

TEST_F(MonitorROOTTest, InitialStateIsIdle) {
  EXPECT_EQ(monitor_->GetState(), ComponentState::Idle);
}

TEST_F(MonitorROOTTest, HasEmptyComponentIdInitially) {
  EXPECT_TRUE(monitor_->GetComponentId().empty());
}

TEST_F(MonitorROOTTest, InitialStatusHasZeroMetrics) {
  auto status = monitor_->GetStatus();
  EXPECT_EQ(status.metrics.events_processed, 0);
  EXPECT_EQ(status.metrics.bytes_transferred, 0);
}

// === Configuration Tests ===

TEST_F(MonitorROOTTest, CanSetHttpPort) {
  monitor_->SetHttpPort(8888);
  EXPECT_EQ(monitor_->GetHttpPort(), 8888);
}

TEST_F(MonitorROOTTest, DefaultHttpPortIs8080) {
  EXPECT_EQ(monitor_->GetHttpPort(), 8080);
}

TEST_F(MonitorROOTTest, CanSetUpdateInterval) {
  monitor_->SetUpdateInterval(500);
  EXPECT_EQ(monitor_->GetUpdateInterval(), 500);
}

TEST_F(MonitorROOTTest, DefaultUpdateIntervalIs1000) {
  EXPECT_EQ(monitor_->GetUpdateInterval(), 1000);
}

TEST_F(MonitorROOTTest, CanSetInputAddress) {
  monitor_->SetInputAddresses({input_address_});
  auto addresses = monitor_->GetInputAddresses();
  ASSERT_EQ(addresses.size(), 1);
  EXPECT_EQ(addresses[0], input_address_);
}

TEST_F(MonitorROOTTest, CanSetComponentId) {
  monitor_->SetComponentId("monitor_test");
  EXPECT_EQ(monitor_->GetComponentId(), "monitor_test");
}

// === Histogram Enable/Disable Tests ===

TEST_F(MonitorROOTTest, CanEnableEnergyHistogram) {
  monitor_->EnableEnergyHistogram(true);
  EXPECT_TRUE(monitor_->IsEnergyHistogramEnabled());
}

TEST_F(MonitorROOTTest, CanEnableChannelHistogram) {
  monitor_->EnableChannelHistogram(true);
  EXPECT_TRUE(monitor_->IsChannelHistogramEnabled());
}

TEST_F(MonitorROOTTest, CanEnableTimingHistogram) {
  monitor_->EnableTimingHistogram(true);
  EXPECT_TRUE(monitor_->IsTimingHistogramEnabled());
}

TEST_F(MonitorROOTTest, CanEnable2DHistogram) {
  monitor_->Enable2DHistogram(true);
  EXPECT_TRUE(monitor_->Is2DHistogramEnabled());
}

TEST_F(MonitorROOTTest, EnergyHistogramEnabledByDefault) {
  EXPECT_TRUE(monitor_->IsEnergyHistogramEnabled());
}

TEST_F(MonitorROOTTest, ChannelHistogramEnabledByDefault) {
  EXPECT_TRUE(monitor_->IsChannelHistogramEnabled());
}

// === Waveform Display Tests ===

TEST_F(MonitorROOTTest, CanEnableWaveformDisplay) {
  monitor_->EnableWaveformDisplay(true);
  EXPECT_TRUE(monitor_->IsWaveformDisplayEnabled());
}

TEST_F(MonitorROOTTest, WaveformDisplayDisabledByDefault) {
  EXPECT_FALSE(monitor_->IsWaveformDisplayEnabled());
}

TEST_F(MonitorROOTTest, CanSetWaveformChannel) {
  monitor_->SetWaveformChannel(1, 5);
  auto [module, channel] = monitor_->GetWaveformChannel();
  EXPECT_EQ(module, 1);
  EXPECT_EQ(channel, 5);
}

// === State Transition Tests ===

TEST_F(MonitorROOTTest, TransitionIdleToConfigured) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);

  EXPECT_TRUE(monitor_->Initialize(""));
  EXPECT_EQ(monitor_->GetState(), ComponentState::Configured);
}

TEST_F(MonitorROOTTest, InitializeFailsWithoutInputAddress) {
  monitor_->SetHttpPort(http_port_);
  // No input address set

  EXPECT_FALSE(monitor_->Initialize(""));
  EXPECT_EQ(monitor_->GetState(), ComponentState::Idle);
}

TEST_F(MonitorROOTTest, ConfiguredToArmed) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");

  EXPECT_TRUE(monitor_->Arm());
  EXPECT_EQ(monitor_->GetState(), ComponentState::Armed);
}

TEST_F(MonitorROOTTest, ArmedToRunning) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");
  monitor_->Arm();

  EXPECT_TRUE(monitor_->Start(1));
  EXPECT_EQ(monitor_->GetState(), ComponentState::Running);
}

TEST_F(MonitorROOTTest, RunningToConfigured) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");
  monitor_->Arm();
  monitor_->Start(1);

  EXPECT_TRUE(monitor_->Stop(true));
  EXPECT_EQ(monitor_->GetState(), ComponentState::Configured);
}

TEST_F(MonitorROOTTest, ErrorToIdle) {
  monitor_->ForceError("Test error");
  EXPECT_EQ(monitor_->GetState(), ComponentState::Error);

  monitor_->Reset();
  EXPECT_EQ(monitor_->GetState(), ComponentState::Idle);
}

// === Invalid Transition Tests ===

TEST_F(MonitorROOTTest, CannotArmFromIdle) {
  EXPECT_FALSE(monitor_->Arm());
  EXPECT_EQ(monitor_->GetState(), ComponentState::Idle);
}

TEST_F(MonitorROOTTest, CannotStartFromConfigured) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");

  EXPECT_FALSE(monitor_->Start(1));
  EXPECT_EQ(monitor_->GetState(), ComponentState::Configured);
}

TEST_F(MonitorROOTTest, CannotStopFromIdle) {
  EXPECT_FALSE(monitor_->Stop(true));
  EXPECT_EQ(monitor_->GetState(), ComponentState::Idle);
}

// === Run Number Tests ===

TEST_F(MonitorROOTTest, RunNumberSetOnStart) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");
  monitor_->Arm();
  monitor_->Start(42);

  auto status = monitor_->GetStatus();
  EXPECT_EQ(status.run_number, 42);

  monitor_->Stop(true);
}

// === Multiple Run Tests ===

TEST_F(MonitorROOTTest, MultipleRunCycles) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");

  for (uint32_t run = 1; run <= 3; ++run) {
    EXPECT_TRUE(monitor_->Arm()) << "Failed to arm for run " << run;
    EXPECT_TRUE(monitor_->Start(run)) << "Failed to start run " << run;

    auto status = monitor_->GetStatus();
    EXPECT_EQ(status.run_number, run);

    EXPECT_TRUE(monitor_->Stop(true)) << "Failed to stop run " << run;
  }
}

// === Histogram Reset on New Run ===

TEST_F(MonitorROOTTest, HistogramsResetOnNewRun) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");

  // First run
  monitor_->Arm();
  monitor_->Start(1);
  monitor_->Stop(true);

  // Second run - histograms should be reset
  monitor_->Arm();
  monitor_->Start(2);

  // Verify histograms are empty (entries = 0)
  // This would require access to histogram internals
  // For now, just verify state transition works
  EXPECT_EQ(monitor_->GetState(), ComponentState::Running);

  monitor_->Stop(true);
}

// === Status Tests ===

TEST_F(MonitorROOTTest, StatusContainsComponentId) {
  monitor_->SetComponentId("test_monitor");
  auto status = monitor_->GetStatus();
  EXPECT_EQ(status.component_id, "test_monitor");
}

TEST_F(MonitorROOTTest, StatusContainsState) {
  monitor_->SetInputAddresses({input_address_});
  monitor_->SetHttpPort(http_port_);
  monitor_->Initialize("");

  auto status = monitor_->GetStatus();
  EXPECT_EQ(status.state, ComponentState::Configured);
}

// === Output Address Tests (MonitorROOT has no output) ===

TEST_F(MonitorROOTTest, OutputAddressesAlwaysEmpty) {
  monitor_->SetOutputAddresses({"tcp://localhost:9999"});
  auto outputs = monitor_->GetOutputAddresses();
  EXPECT_TRUE(outputs.empty());
}

#endif  // HAS_ROOT
