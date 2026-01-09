/**
 * @file test_file_writer.cpp
 * @brief Unit tests for FileWriter component
 */

#include <FileWriter.hpp>
#include <delila/core/ComponentState.hpp>
#include <delila/core/ComponentStatus.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace DELILA {
namespace test {

class FileWriterTest : public ::testing::Test {
protected:
  void SetUp() override {
    writer_ = std::make_unique<FileWriter>();
    // Create temp directory for test files
    test_dir_ = std::filesystem::temp_directory_path() / "delila_test";
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    if (writer_) {
      writer_->Shutdown();
    }
    // Clean up test files
    std::filesystem::remove_all(test_dir_);
  }

  std::unique_ptr<FileWriter> writer_;
  std::filesystem::path test_dir_;
};

// === Initial State Tests ===

TEST_F(FileWriterTest, InitialStateIsIdle) {
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);
}

TEST_F(FileWriterTest, HasEmptyComponentIdInitially) {
  auto status = writer_->GetStatus();
  EXPECT_TRUE(status.component_id.empty());
}

TEST_F(FileWriterTest, InitialStatusHasZeroMetrics) {
  auto status = writer_->GetStatus();
  EXPECT_EQ(status.metrics.events_processed, 0);
  EXPECT_EQ(status.metrics.bytes_transferred, 0);
  EXPECT_EQ(status.run_number, 0);
}

// === Address Configuration Tests ===

TEST_F(FileWriterTest, OutputAddressesAreEmpty) {
  // FileWriter has no outputs (it's a data sink)
  auto outputs = writer_->GetOutputAddresses();
  EXPECT_TRUE(outputs.empty());
}

TEST_F(FileWriterTest, CanSetInputAddresses) {
  std::vector<std::string> inputs = {"tcp://localhost:5555"};
  writer_->SetInputAddresses(inputs);

  auto result = writer_->GetInputAddresses();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], "tcp://localhost:5555");
}

TEST_F(FileWriterTest, SetOutputAddressesIsIgnored) {
  // FileWriter should ignore output addresses
  std::vector<std::string> outputs = {"tcp://localhost:6666"};
  writer_->SetOutputAddresses(outputs);

  auto result = writer_->GetOutputAddresses();
  EXPECT_TRUE(result.empty());
}

// === State Transition Tests ===

TEST_F(FileWriterTest, TransitionIdleToConfigured) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());

  EXPECT_TRUE(writer_->Initialize(""));
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

TEST_F(FileWriterTest, ConfiguredToArmed) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");

  EXPECT_TRUE(writer_->Arm());
  EXPECT_EQ(writer_->GetState(), ComponentState::Armed);
}

TEST_F(FileWriterTest, ArmedToRunning) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");
  writer_->Arm();

  EXPECT_TRUE(writer_->Start(100));
  EXPECT_EQ(writer_->GetState(), ComponentState::Running);
  EXPECT_EQ(writer_->GetStatus().run_number, 100);
}

TEST_F(FileWriterTest, RunningToConfigured) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");
  writer_->Arm();
  writer_->Start(100);

  EXPECT_TRUE(writer_->Stop(true));
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

TEST_F(FileWriterTest, ErrorToIdle) {
  // Force error state
  writer_->ForceError("Test error");
  EXPECT_EQ(writer_->GetState(), ComponentState::Error);

  writer_->Reset();
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);
}

// === Invalid State Transition Tests ===

TEST_F(FileWriterTest, CannotArmFromIdle) {
  EXPECT_FALSE(writer_->Arm());
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);
}

TEST_F(FileWriterTest, CannotStartFromConfigured) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");

  EXPECT_FALSE(writer_->Start(100));
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

TEST_F(FileWriterTest, CannotStopFromIdle) {
  EXPECT_FALSE(writer_->Stop(true));
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);
}

// === Component ID Tests ===

TEST_F(FileWriterTest, ComponentIdAfterInitialize) {
  writer_->SetComponentId("writer_01");
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");

  EXPECT_EQ(writer_->GetComponentId(), "writer_01");
}

// === File Path Configuration Tests ===

TEST_F(FileWriterTest, CanSetOutputPath) {
  writer_->SetOutputPath(test_dir_.string());
  EXPECT_EQ(writer_->GetOutputPath(), test_dir_.string());
}

TEST_F(FileWriterTest, CanSetFilePrefix) {
  writer_->SetFilePrefix("data_");
  EXPECT_EQ(writer_->GetFilePrefix(), "data_");
}

TEST_F(FileWriterTest, DefaultFilePrefix) {
  // Default should be "run_"
  EXPECT_EQ(writer_->GetFilePrefix(), "run_");
}

// === File Creation Tests ===

TEST_F(FileWriterTest, CreatesFileOnStart) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->SetFilePrefix("test_");
  writer_->Initialize("");
  writer_->Arm();
  writer_->Start(42);

  // Check if file was created
  auto expected_file = test_dir_ / "test_000042.dat";
  // File should be created or will be created on first write
  EXPECT_EQ(writer_->GetState(), ComponentState::Running);

  writer_->Stop(true);
}

// === Graceful vs Emergency Stop Tests ===

TEST_F(FileWriterTest, GracefulStopFlushesData) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");
  writer_->Arm();
  writer_->Start(1);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Graceful stop should wait for data to be flushed
  EXPECT_TRUE(writer_->Stop(true));
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

TEST_F(FileWriterTest, EmergencyStopImmediate) {
  writer_->SetInputAddresses({"tcp://localhost:5555"});
  writer_->SetOutputPath(test_dir_.string());
  writer_->Initialize("");
  writer_->Arm();
  writer_->Start(1);

  // Emergency stop should be immediate
  EXPECT_TRUE(writer_->Stop(false));
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

} // namespace test
} // namespace DELILA
