/**
 * @file test_operator_component_integration.cpp
 * @brief Integration tests for CLIOperator ↔ DataComponents ZMQ communication
 *
 * Phase 5: Tests CLIOperator controlling DataComponents via ZMQ REQ/REP commands.
 *
 * Architecture:
 *   CLIOperator (REQ) ←→ DataComponent (REP)
 *
 * Test scenarios:
 * 1. Single component command/response
 * 2. Multiple components orchestration
 * 3. Full pipeline control
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

#include "CLIOperator.hpp"
#include "DigitizerSource.hpp"
#include "FileWriter.hpp"
#include "SimpleMerger.hpp"
#include "delila/core/Command.hpp"
#include "delila/core/CommandResponse.hpp"
#include "delila/core/ComponentState.hpp"
#include "test_utils.hpp"

using namespace DELILA;
using namespace DELILA::test;
using namespace std::chrono_literals;

// Port manager to avoid conflicts between tests
class OperatorIntegrationPortManager {
 private:
  static std::atomic<int> next_port_;

 public:
  static int GetNextPort() { return next_port_.fetch_add(1); }
};

std::atomic<int> OperatorIntegrationPortManager::next_port_{39000};

// === Single Component Control Tests ===

class SingleComponentControlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get unique ports for this test
    command_port_ = OperatorIntegrationPortManager::GetNextPort();
    data_port_ = OperatorIntegrationPortManager::GetNextPort();

    command_address_ = "tcp://127.0.0.1:" + std::to_string(command_port_);
    data_address_ = "tcp://127.0.0.1:" + std::to_string(data_port_);

    // Create temp directory for output files
    temp_dir_ = std::filesystem::temp_directory_path() / "delila_operator_test";
    std::filesystem::create_directories(temp_dir_);

    operator_ = std::make_unique<CLIOperator>();
    source_ = std::make_unique<DigitizerSource>();
  }

  void TearDown() override {
    if (source_) {
      source_->Shutdown();
      source_.reset();
    }
    if (operator_) {
      operator_->Shutdown();
      operator_.reset();
    }

    WaitForConnection(50);

    std::error_code ec;
    std::filesystem::remove_all(temp_dir_, ec);
  }

  void ConfigureSource() {
    source_->SetMockMode(true);
    source_->SetMockEventRate(500);
    source_->SetComponentId("test_source");
    source_->SetOutputAddresses({data_address_});
    source_->SetCommandAddress(command_address_);
  }

  void RegisterSourceWithOperator() {
    ComponentAddress addr;
    addr.component_id = "test_source";
    addr.command_address = command_address_;
    addr.component_type = "DigitizerSource";
    addr.start_order = 0;
    operator_->RegisterComponent(addr);
  }

  int command_port_;
  int data_port_;
  std::string command_address_;
  std::string data_address_;
  std::filesystem::path temp_dir_;
  std::unique_ptr<CLIOperator> operator_;
  std::unique_ptr<DigitizerSource> source_;
};

// Test: Operator can send Configure command to component
TEST_F(SingleComponentControlTest, OperatorSendsConfigureCommand) {
  ConfigureSource();
  RegisterSourceWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  // Source starts command listener
  ASSERT_TRUE(source_->Initialize(""));
  source_->StartCommandListener();

  // Wait for command socket to be ready
  WaitForSocketSetup(100);

  // Operator sends Configure command
  auto job_id = operator_->ConfigureAllAsync();
  ASSERT_FALSE(job_id.empty());

  // Wait for job to complete
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(job_id).state == JobState::Completed;
      },
      3000));

  // Verify component received command and is now Configured
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

// Test: Operator can send Arm command to component
TEST_F(SingleComponentControlTest, OperatorSendsArmCommand) {
  ConfigureSource();
  RegisterSourceWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  // Source starts and becomes Configured
  ASSERT_TRUE(source_->Initialize(""));
  source_->StartCommandListener();

  WaitForSocketSetup(30);

  // First Configure
  auto config_job = operator_->ConfigureAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(config_job).state == JobState::Completed;
      },
      2000));

  // Then Arm
  auto arm_job = operator_->ArmAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(arm_job).state == JobState::Completed;
      },
      2000));

  // Verify component is Armed
  EXPECT_EQ(source_->GetState(), ComponentState::Armed);
}

// Test: Operator can send Start command with run number
TEST_F(SingleComponentControlTest, OperatorSendsStartCommand) {
  ConfigureSource();
  RegisterSourceWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  ASSERT_TRUE(source_->Initialize(""));
  source_->StartCommandListener();

  WaitForSocketSetup(30);

  // Configure → Arm → Start sequence
  auto config_job = operator_->ConfigureAllAsync();
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(config_job).state == JobState::Completed;
      },
      2000);

  auto arm_job = operator_->ArmAllAsync();
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(arm_job).state == JobState::Completed;
      },
      2000);

  uint32_t run_number = 12345;
  auto start_job = operator_->StartAllAsync(run_number);
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(start_job).state == JobState::Completed;
      },
      2000));

  // Verify component is Running with correct run number
  EXPECT_EQ(source_->GetState(), ComponentState::Running);
  EXPECT_EQ(source_->GetStatus().run_number, run_number);
}

// Test: Operator can send Stop command
TEST_F(SingleComponentControlTest, OperatorSendsStopCommand) {
  ConfigureSource();
  RegisterSourceWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  ASSERT_TRUE(source_->Initialize(""));
  source_->StartCommandListener();

  WaitForSocketSetup(30);

  // Full lifecycle: Configure → Arm → Start → Stop
  auto config_job = operator_->ConfigureAllAsync();
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(config_job).state == JobState::Completed;
      },
      2000);

  auto arm_job = operator_->ArmAllAsync();
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(arm_job).state == JobState::Completed;
      },
      2000);

  auto start_job = operator_->StartAllAsync(1);
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(start_job).state == JobState::Completed;
      },
      2000);

  // Wait for Running state
  WaitForRunning(*source_, 100);

  auto stop_job = operator_->StopAllAsync(true);
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(stop_job).state == JobState::Completed;
      },
      2000));

  // Verify component is back to Configured
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

// Test: Full lifecycle through Operator
TEST_F(SingleComponentControlTest, FullLifecycleThroughOperator) {
  ConfigureSource();
  RegisterSourceWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  ASSERT_TRUE(source_->Initialize(""));
  source_->StartCommandListener();

  WaitForSocketSetup(30);

  // Configure
  auto config_job = operator_->ConfigureAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(config_job).state == JobState::Completed;
      },
      2000));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);

  // Arm
  auto arm_job = operator_->ArmAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(arm_job).state == JobState::Completed;
      },
      2000));
  EXPECT_EQ(source_->GetState(), ComponentState::Armed);

  // Start
  auto start_job = operator_->StartAllAsync(100);
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(start_job).state == JobState::Completed;
      },
      2000));
  EXPECT_EQ(source_->GetState(), ComponentState::Running);

  // Let it run briefly
  WaitForCondition(
      [&]() { return source_->GetStatus().metrics.events_processed > 0; }, 500);

  // Stop
  auto stop_job = operator_->StopAllAsync(true);
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(stop_job).state == JobState::Completed;
      },
      2000));
  EXPECT_EQ(source_->GetState(), ComponentState::Configured);

  // Reset
  auto reset_job = operator_->ResetAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(reset_job).state == JobState::Completed;
      },
      2000));
  EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

// === Multiple Component Control Tests ===

class MultipleComponentControlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get unique ports
    source_command_port_ = OperatorIntegrationPortManager::GetNextPort();
    writer_command_port_ = OperatorIntegrationPortManager::GetNextPort();
    data_port_ = OperatorIntegrationPortManager::GetNextPort();

    source_command_address_ =
        "tcp://127.0.0.1:" + std::to_string(source_command_port_);
    writer_command_address_ =
        "tcp://127.0.0.1:" + std::to_string(writer_command_port_);
    data_address_ = "tcp://127.0.0.1:" + std::to_string(data_port_);

    temp_dir_ =
        std::filesystem::temp_directory_path() / "delila_multi_operator_test";
    std::filesystem::create_directories(temp_dir_);

    operator_ = std::make_unique<CLIOperator>();
    source_ = std::make_unique<DigitizerSource>();
    writer_ = std::make_unique<FileWriter>();
  }

  void TearDown() override {
    if (writer_) {
      writer_->Shutdown();
      writer_.reset();
    }
    if (source_) {
      source_->Shutdown();
      source_.reset();
    }
    if (operator_) {
      operator_->Shutdown();
      operator_.reset();
    }

    WaitForConnection(50);

    std::error_code ec;
    std::filesystem::remove_all(temp_dir_, ec);
  }

  void ConfigureComponents() {
    // Source
    source_->SetMockMode(true);
    source_->SetMockEventRate(500);
    source_->SetComponentId("source_01");
    source_->SetOutputAddresses({data_address_});
    source_->SetCommandAddress(source_command_address_);

    // Writer
    writer_->SetComponentId("writer_01");
    writer_->SetInputAddresses({data_address_});
    writer_->SetOutputPath(temp_dir_.string());
    writer_->SetFilePrefix("operator_test_");
    writer_->SetCommandAddress(writer_command_address_);
  }

  void RegisterComponentsWithOperator() {
    // Register source (start_order = 0, starts first)
    ComponentAddress source_addr;
    source_addr.component_id = "source_01";
    source_addr.command_address = source_command_address_;
    source_addr.component_type = "DigitizerSource";
    source_addr.start_order = 0;
    operator_->RegisterComponent(source_addr);

    // Register writer (start_order = 1, starts second)
    ComponentAddress writer_addr;
    writer_addr.component_id = "writer_01";
    writer_addr.command_address = writer_command_address_;
    writer_addr.component_type = "FileWriter";
    writer_addr.start_order = 1;
    operator_->RegisterComponent(writer_addr);
  }

  int source_command_port_;
  int writer_command_port_;
  int data_port_;
  std::string source_command_address_;
  std::string writer_command_address_;
  std::string data_address_;
  std::filesystem::path temp_dir_;
  std::unique_ptr<CLIOperator> operator_;
  std::unique_ptr<DigitizerSource> source_;
  std::unique_ptr<FileWriter> writer_;
};

// Test: Operator controls multiple components
TEST_F(MultipleComponentControlTest, OperatorControlsMultipleComponents) {
  ConfigureComponents();
  RegisterComponentsWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  // Initialize and start command listeners
  ASSERT_TRUE(source_->Initialize(""));
  ASSERT_TRUE(writer_->Initialize(""));
  source_->StartCommandListener();
  writer_->StartCommandListener();

  WaitForSocketSetup(50);

  // Configure all
  auto config_job = operator_->ConfigureAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(config_job).state == JobState::Completed;
      },
      3000));

  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);

  // Arm all
  auto arm_job = operator_->ArmAllAsync();
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(arm_job).state == JobState::Completed;
      },
      3000));

  EXPECT_EQ(source_->GetState(), ComponentState::Armed);
  EXPECT_EQ(writer_->GetState(), ComponentState::Armed);

  // Start all
  auto start_job = operator_->StartAllAsync(42);
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(start_job).state == JobState::Completed;
      },
      3000));

  EXPECT_EQ(source_->GetState(), ComponentState::Running);
  EXPECT_EQ(writer_->GetState(), ComponentState::Running);

  // Let it run briefly
  std::this_thread::sleep_for(100ms);

  // Stop all
  auto stop_job = operator_->StopAllAsync(true);
  EXPECT_TRUE(WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(stop_job).state == JobState::Completed;
      },
      3000));

  EXPECT_EQ(source_->GetState(), ComponentState::Configured);
  EXPECT_EQ(writer_->GetState(), ComponentState::Configured);
}

// Test: Operator reports component status
TEST_F(MultipleComponentControlTest, OperatorReportsComponentStatus) {
  ConfigureComponents();
  RegisterComponentsWithOperator();

  operator_->SetComponentId("test_operator");
  ASSERT_TRUE(operator_->Initialize(""));

  ASSERT_TRUE(source_->Initialize(""));
  ASSERT_TRUE(writer_->Initialize(""));
  source_->StartCommandListener();
  writer_->StartCommandListener();

  WaitForSocketSetup(50);

  // Configure and start
  auto config_job = operator_->ConfigureAllAsync();
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(config_job).state == JobState::Completed;
      },
      3000);

  auto arm_job = operator_->ArmAllAsync();
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(arm_job).state == JobState::Completed;
      },
      3000);

  auto start_job = operator_->StartAllAsync(1);
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(start_job).state == JobState::Completed;
      },
      3000);

  // Check that operator can query all component status
  auto all_status = operator_->GetAllComponentStatus();
  EXPECT_EQ(all_status.size(), 2);

  // Check IsAllInState
  EXPECT_TRUE(operator_->IsAllInState(ComponentState::Running));

  // Stop
  auto stop_job = operator_->StopAllAsync(true);
  WaitForCondition(
      [&]() {
        return operator_->GetJobStatus(stop_job).state == JobState::Completed;
      },
      3000);

  EXPECT_TRUE(operator_->IsAllInState(ComponentState::Configured));
}

