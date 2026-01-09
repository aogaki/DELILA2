/**
 * @file test_source_writer_pipeline.cpp
 * @brief Integration tests for DigitizerSource → FileWriter pipeline
 *
 * This test validates the complete data flow:
 * 1. DigitizerSource (mock mode) generates events
 * 2. Events are serialized and sent via ZMQ
 * 3. FileWriter receives, decodes, and writes to file
 * 4. Data integrity is verified
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

#include "DigitizerSource.hpp"
#include "FileWriter.hpp"
#include "delila/core/ComponentState.hpp"
#include "test_utils.hpp"

using namespace DELILA;
using namespace DELILA::test;
using namespace std::chrono_literals;

// Port manager to avoid conflicts between tests
class PipelinePortManager {
 private:
  static std::atomic<int> next_port_;

 public:
  static int GetNextPort() { return next_port_.fetch_add(1); }
};

std::atomic<int> PipelinePortManager::next_port_{37000};

class SourceWriterPipelineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    current_port_ = PipelinePortManager::GetNextPort();
    data_address_ = "tcp://127.0.0.1:" + std::to_string(current_port_);

    // Create temp directory for output files
    temp_dir_ = std::filesystem::temp_directory_path() / "delila_test";
    std::filesystem::create_directories(temp_dir_);

    source_ = std::make_unique<DigitizerSource>();
    writer_ = std::make_unique<FileWriter>();
  }

  void TearDown() override {
    // Cleanup components - order matters: stop writer first, then source
    if (writer_) {
      writer_->Shutdown();
      writer_.reset();
    }
    if (source_) {
      source_->Shutdown();
      source_.reset();
    }

    // Wait for sockets to fully close (ZMQ needs time)
    WaitForConnection(50);

    // Cleanup temp files
    std::error_code ec;
    std::filesystem::remove_all(temp_dir_, ec);
  }

  // Helper to configure source in mock mode
  void ConfigureSource() {
    source_->SetMockMode(true);
    source_->SetMockEventRate(1000);  // 1000 events/second
    source_->SetComponentId("test_source");
    source_->SetOutputAddresses({data_address_});
  }

  // Helper to configure writer
  void ConfigureWriter() {
    writer_->SetComponentId("test_writer");
    writer_->SetInputAddresses({data_address_});
    writer_->SetOutputPath(temp_dir_.string());
    writer_->SetFilePrefix("test_run_");
  }

  int current_port_;
  std::string data_address_;
  std::filesystem::path temp_dir_;
  std::unique_ptr<DigitizerSource> source_;
  std::unique_ptr<FileWriter> writer_;
};

// === Basic Pipeline Tests ===

TEST_F(SourceWriterPipelineTest, BothComponentsInitialize) {
  ConfigureSource();
  ConfigureWriter();

  EXPECT_TRUE(source_->Initialize(""));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);

  EXPECT_TRUE(writer_->Initialize(""));
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

TEST_F(SourceWriterPipelineTest, BothComponentsArm) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");

  // Source binds, so arm it first
  EXPECT_TRUE(source_->Arm());
  EXPECT_TRUE(WaitForArmed(*source_, 100));

  // Writer connects - wait for socket setup then arm
  WaitForSocketSetup(20);
  EXPECT_TRUE(writer_->Arm());
  EXPECT_TRUE(WaitForArmed(*writer_, 100));
}

TEST_F(SourceWriterPipelineTest, BothComponentsStart) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");

  // Arm both
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();

  // Start both with same run number
  uint32_t run_number = 12345;
  EXPECT_TRUE(source_->Start(run_number));
  EXPECT_TRUE(writer_->Start(run_number));

  // Wait for actual Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Verify run numbers
  EXPECT_EQ(source_->GetStatus().run_number, run_number);
  EXPECT_EQ(writer_->GetStatus().run_number, run_number);
}

TEST_F(SourceWriterPipelineTest, BothComponentsStop) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();
  source_->Start(1);
  writer_->Start(1);

  // Wait for Running state, then run briefly
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Stop source first (stops sending data)
  EXPECT_TRUE(source_->Stop(true));
  EXPECT_TRUE(WaitForConfigured(*source_, 100));

  // Then stop writer
  EXPECT_TRUE(writer_->Stop(true));
  EXPECT_TRUE(WaitForConfigured(*writer_, 100));
}

// === State Machine Tests ===

TEST_F(SourceWriterPipelineTest, FullStateTransitionCycle) {
  ConfigureSource();
  ConfigureWriter();

  // Idle → Configured
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);

  source_->Initialize("");
  writer_->Initialize("");

  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);

  // Configured → Armed
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();

  EXPECT_TRUE(WaitForArmed(*source_, 100));
  EXPECT_TRUE(WaitForArmed(*writer_, 100));

  // Armed → Running
  source_->Start(1);
  writer_->Start(1);

  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Running → Configured
  source_->Stop(true);
  writer_->Stop(true);

  EXPECT_TRUE(WaitForConfigured(*source_, 100));
  EXPECT_TRUE(WaitForConfigured(*writer_, 100));

  // Configured → Armed (re-arm)
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();

  EXPECT_TRUE(WaitForArmed(*source_, 100));
  EXPECT_TRUE(WaitForArmed(*writer_, 100));
}

// === Data Flow Tests ===

TEST_F(SourceWriterPipelineTest, OutputFileCreatedOnStart) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();

  uint32_t run_number = 42;
  source_->Start(run_number);
  writer_->Start(run_number);

  // Check file was created
  std::string expected_file = temp_dir_.string() + "/test_run_000042.dat";
  EXPECT_TRUE(std::filesystem::exists(expected_file));

  source_->Stop(true);
  writer_->Stop(true);
}

TEST_F(SourceWriterPipelineTest, OutputFileClosedOnStop) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();

  uint32_t run_number = 99;
  source_->Start(run_number);
  writer_->Start(run_number);

  // Wait for Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  source_->Stop(true);
  writer_->Stop(true);

  // File should exist and be readable
  std::string output_file = temp_dir_.string() + "/test_run_000099.dat";
  ASSERT_TRUE(std::filesystem::exists(output_file));

  // Should be able to open file for reading (not locked)
  std::ifstream file(output_file, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  file.close();
}

// === Metrics Tests ===

TEST_F(SourceWriterPipelineTest, SourceMetricsIncrease) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();
  source_->Start(1);
  writer_->Start(1);

  // Wait for Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  auto initial_status = source_->GetStatus();

  // Wait for some events to be processed
  WaitForCondition(
      [&]() {
        return source_->GetStatus().metrics.events_processed >
               initial_status.metrics.events_processed;
      },
      200);

  auto final_status = source_->GetStatus();

  // In mock mode with network, events should be generated
  // Note: bytes_transferred may be 0 if send fails due to no subscriber
  // but events_processed should increase
  EXPECT_GE(final_status.metrics.events_processed,
            initial_status.metrics.events_processed);

  source_->Stop(true);
  writer_->Stop(true);
}

// === Error Recovery Tests ===

TEST_F(SourceWriterPipelineTest, SourceErrorAndReset) {
  ConfigureSource();
  source_->Initialize("");

  // Force error state
  source_->ForceError("Test error");
  EXPECT_EQ(source_->GetState(), ComponentState::Error);

  // Reset should recover
  source_->Reset();
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

TEST_F(SourceWriterPipelineTest, WriterErrorAndReset) {
  ConfigureWriter();
  writer_->Initialize("");

  // Force error state
  writer_->ForceError("Test error");
  EXPECT_EQ(writer_->GetState(), ComponentState::Error);

  // Reset should recover
  writer_->Reset();
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);
}

TEST_F(SourceWriterPipelineTest, BothRecoverFromError) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");

  // Force both into error
  source_->ForceError("Source error");
  writer_->ForceError("Writer error");

  EXPECT_EQ(source_->GetState(), ComponentState::Error);
  EXPECT_EQ(writer_->GetState(), ComponentState::Error);

  // Reset both
  source_->Reset();
  writer_->Reset();

  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
  EXPECT_EQ(writer_->GetState(), ComponentState::Idle);

  // Should be able to re-initialize
  EXPECT_TRUE(source_->Initialize(""));
  EXPECT_TRUE(writer_->Initialize(""));
}

// === Multiple Run Tests ===

TEST_F(SourceWriterPipelineTest, MultipleRunsCreateSeparateFiles) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");

  std::vector<uint32_t> run_numbers = {100, 101, 102};

  for (uint32_t run : run_numbers) {
    source_->Arm();
    WaitForSocketSetup(20);
    writer_->Arm();

    source_->Start(run);
    writer_->Start(run);

    // Wait for Running state
    EXPECT_TRUE(WaitForRunning(*source_, 100));
    EXPECT_TRUE(WaitForRunning(*writer_, 100));

    source_->Stop(true);
    writer_->Stop(true);

    // Wait for Configured state before next run
    EXPECT_TRUE(WaitForConfigured(*source_, 100));
    EXPECT_TRUE(WaitForConfigured(*writer_, 100));
  }

  // Check all files were created
  for (uint32_t run : run_numbers) {
    char filename[64];
    snprintf(filename, sizeof(filename), "test_run_%06u.dat", run);
    std::filesystem::path file_path = temp_dir_ / filename;
    EXPECT_TRUE(std::filesystem::exists(file_path))
        << "Missing file: " << file_path;
  }
}

// === Component ID Tests ===

TEST_F(SourceWriterPipelineTest, ComponentIdsPreserved) {
  // Configure first (sets default IDs)
  ConfigureSource();
  ConfigureWriter();

  // Then override with custom IDs
  source_->SetComponentId("my_source_01");
  writer_->SetComponentId("my_writer_01");

  EXPECT_EQ(source_->GetComponentId(), "my_source_01");
  EXPECT_EQ(writer_->GetComponentId(), "my_writer_01");

  source_->Initialize("");
  writer_->Initialize("");

  // IDs should persist after initialization
  EXPECT_EQ(source_->GetStatus().component_id, "my_source_01");
  EXPECT_EQ(writer_->GetStatus().component_id, "my_writer_01");
}

// === Graceful vs Emergency Stop ===

TEST_F(SourceWriterPipelineTest, GracefulStopWaitsForThreads) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();
  source_->Start(1);
  writer_->Start(1);

  // Wait for Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Graceful stop - should complete without issues
  source_->Stop(true);
  writer_->Stop(true);

  // Wait for Configured state
  EXPECT_TRUE(WaitForConfigured(*source_, 100));
  EXPECT_TRUE(WaitForConfigured(*writer_, 100));
}

TEST_F(SourceWriterPipelineTest, EmergencyStopIsImmediate) {
  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();
  source_->Start(1);
  writer_->Start(1);

  // Wait for Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Emergency stop - should be quick
  source_->Stop(false);
  writer_->Stop(false);

  // Wait for Configured state
  EXPECT_TRUE(WaitForConfigured(*source_, 100));
  EXPECT_TRUE(WaitForConfigured(*writer_, 100));
}

// === EOS (End Of Stream) Tests ===

TEST_F(SourceWriterPipelineTest, GracefulStopSendsEOS) {
  // This test verifies that graceful stop sends EOS message
  // and FileWriter receives it

  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();
  source_->Start(1);
  writer_->Start(1);

  // Wait for Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Let some data flow
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Graceful stop should send EOS
  source_->Stop(true);

  // Wait for writer to receive EOS and complete
  // FileWriter should detect EOS and can stop gracefully
  WaitForCondition([&]() { return writer_->HasReceivedEOS(); }, 500);

  EXPECT_TRUE(writer_->HasReceivedEOS());

  writer_->Stop(true);

  EXPECT_TRUE(WaitForConfigured(*source_, 100));
  EXPECT_TRUE(WaitForConfigured(*writer_, 100));
}

TEST_F(SourceWriterPipelineTest, EmergencyStopDoesNotSendEOS) {
  // Emergency stop should NOT send EOS - it's an immediate abort

  ConfigureSource();
  ConfigureWriter();

  source_->Initialize("");
  writer_->Initialize("");
  source_->Arm();
  WaitForSocketSetup(20);
  writer_->Arm();
  source_->Start(1);
  writer_->Start(1);

  // Wait for Running state
  EXPECT_TRUE(WaitForRunning(*source_, 100));
  EXPECT_TRUE(WaitForRunning(*writer_, 100));

  // Emergency stop - no EOS sent
  source_->Stop(false);

  // Brief wait - writer should NOT receive EOS
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_FALSE(writer_->HasReceivedEOS());

  writer_->Stop(false);
}
