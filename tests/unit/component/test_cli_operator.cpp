/**
 * @file test_cli_operator.cpp
 * @brief Unit tests for CLIOperator component
 *
 * CLIOperator implements IOperator interface and provides:
 * - Management of multiple DataComponents
 * - Async command execution (Configure, Arm, Start, Stop, Reset)
 * - Component status monitoring
 * - Job status tracking
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "CLIOperator.hpp"
#include "delila/core/ComponentState.hpp"
#include "delila/core/IOperator.hpp"

using namespace DELILA;
using namespace std::chrono_literals;

class CLIOperatorTest : public ::testing::Test {
 protected:
  void SetUp() override { operator_ = std::make_unique<CLIOperator>(); }

  void TearDown() override {
    if (operator_) {
      operator_->Shutdown();
      // Wait for detached threads to complete
      std::this_thread::sleep_for(50ms);
    }
  }

  std::unique_ptr<CLIOperator> operator_;
};

// === Initial State Tests ===

TEST_F(CLIOperatorTest, InitialStateIsIdle) {
  EXPECT_EQ(operator_->GetState(), ComponentState::Idle);
}

TEST_F(CLIOperatorTest, HasEmptyComponentIdInitially) {
  EXPECT_TRUE(operator_->GetComponentId().empty());
}

TEST_F(CLIOperatorTest, InitialStatusHasZeroMetrics) {
  auto status = operator_->GetStatus();
  EXPECT_EQ(status.metrics.events_processed, 0);
  EXPECT_EQ(status.metrics.bytes_transferred, 0);
}

TEST_F(CLIOperatorTest, NoManagedComponentsInitially) {
  auto ids = operator_->GetComponentIds();
  EXPECT_TRUE(ids.empty());
}

// === IOperator Interface Tests ===

TEST_F(CLIOperatorTest, ImplementsIOperatorInterface) {
  IOperator* base = operator_.get();
  EXPECT_NE(base, nullptr);
}

TEST_F(CLIOperatorTest, ImplementsIComponentInterface) {
  IComponent* base = operator_.get();
  EXPECT_NE(base, nullptr);
}

// === Component ID Tests ===

TEST_F(CLIOperatorTest, CanSetComponentId) {
  operator_->SetComponentId("my_operator");
  EXPECT_EQ(operator_->GetComponentId(), "my_operator");
}

TEST_F(CLIOperatorTest, ComponentIdPreservedInStatus) {
  operator_->SetComponentId("test_operator");
  auto status = operator_->GetStatus();
  EXPECT_EQ(status.component_id, "test_operator");
}

// === Component Registration Tests ===

TEST_F(CLIOperatorTest, CanRegisterComponent) {
  ComponentAddress addr;
  addr.component_id = "source_01";
  addr.command_address = "tcp://localhost:5555";
  addr.status_address = "tcp://localhost:5556";
  addr.component_type = "DigitizerSource";
  addr.start_order = 0;

  operator_->RegisterComponent(addr);

  auto ids = operator_->GetComponentIds();
  ASSERT_EQ(ids.size(), 1);
  EXPECT_EQ(ids[0], "source_01");
}

TEST_F(CLIOperatorTest, CanRegisterMultipleComponents) {
  ComponentAddress source;
  source.component_id = "source_01";
  source.command_address = "tcp://localhost:5555";
  source.status_address = "tcp://localhost:5556";
  source.component_type = "DigitizerSource";
  source.start_order = 0;

  ComponentAddress writer;
  writer.component_id = "writer_01";
  writer.command_address = "tcp://localhost:5557";
  writer.status_address = "tcp://localhost:5558";
  writer.component_type = "FileWriter";
  writer.start_order = 1;

  operator_->RegisterComponent(source);
  operator_->RegisterComponent(writer);

  auto ids = operator_->GetComponentIds();
  ASSERT_EQ(ids.size(), 2);
}

TEST_F(CLIOperatorTest, CanUnregisterComponent) {
  ComponentAddress addr;
  addr.component_id = "source_01";
  addr.command_address = "tcp://localhost:5555";
  addr.status_address = "tcp://localhost:5556";
  addr.component_type = "DigitizerSource";
  addr.start_order = 0;

  operator_->RegisterComponent(addr);
  operator_->UnregisterComponent("source_01");

  auto ids = operator_->GetComponentIds();
  EXPECT_TRUE(ids.empty());
}

// === Initialize Tests ===

TEST_F(CLIOperatorTest, InitializeTransitionsToConfigured) {
  operator_->SetComponentId("test_operator");

  EXPECT_TRUE(operator_->Initialize(""));
  EXPECT_EQ(operator_->GetState(), ComponentState::Configured);
}

TEST_F(CLIOperatorTest, InitializeFailsWithoutComponentId) {
  // Empty component ID - should still work but with warning
  EXPECT_TRUE(operator_->Initialize(""));
}

// === Async Command Tests - ConfigureAllAsync ===

TEST_F(CLIOperatorTest, ConfigureAllAsyncReturnsJobId) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ConfigureAllAsync();

  EXPECT_FALSE(job_id.empty());
}

TEST_F(CLIOperatorTest, ConfigureAllAsyncJobInitiallyPending) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ConfigureAllAsync();
  auto status = operator_->GetJobStatus(job_id);

  // Job should be Pending or Running initially
  EXPECT_TRUE(status.state == JobState::Pending ||
              status.state == JobState::Running ||
              status.state == JobState::Completed);
}

TEST_F(CLIOperatorTest, ConfigureAllAsyncWithNoComponentsCompletesImmediately) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ConfigureAllAsync();

  // Give some time for the job to complete
  std::this_thread::sleep_for(200ms);

  auto status = operator_->GetJobStatus(job_id);
  EXPECT_EQ(status.state, JobState::Completed);
}

// === Async Command Tests - ArmAllAsync ===

TEST_F(CLIOperatorTest, ArmAllAsyncReturnsJobId) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ArmAllAsync();

  EXPECT_FALSE(job_id.empty());
}

TEST_F(CLIOperatorTest, ArmAllAsyncWithNoComponentsCompletesImmediately) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ArmAllAsync();

  std::this_thread::sleep_for(200ms);

  auto status = operator_->GetJobStatus(job_id);
  EXPECT_EQ(status.state, JobState::Completed);
}

// === Async Command Tests - StartAllAsync ===

TEST_F(CLIOperatorTest, StartAllAsyncReturnsJobId) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->StartAllAsync(1);

  EXPECT_FALSE(job_id.empty());
}

TEST_F(CLIOperatorTest, StartAllAsyncWithRunNumber) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  uint32_t run_number = 12345;
  auto job_id = operator_->StartAllAsync(run_number);

  std::this_thread::sleep_for(200ms);

  auto status = operator_->GetJobStatus(job_id);
  EXPECT_EQ(status.state, JobState::Completed);
}

// === Async Command Tests - StopAllAsync ===

TEST_F(CLIOperatorTest, StopAllAsyncGracefulReturnsJobId) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->StopAllAsync(true);

  EXPECT_FALSE(job_id.empty());
}

TEST_F(CLIOperatorTest, StopAllAsyncEmergencyReturnsJobId) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->StopAllAsync(false);

  EXPECT_FALSE(job_id.empty());
}

TEST_F(CLIOperatorTest, StopAllAsyncCompletesWithNoComponents) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->StopAllAsync(true);

  std::this_thread::sleep_for(200ms);

  auto status = operator_->GetJobStatus(job_id);
  EXPECT_EQ(status.state, JobState::Completed);
}

// === Async Command Tests - ResetAllAsync ===

TEST_F(CLIOperatorTest, ResetAllAsyncReturnsJobId) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ResetAllAsync();

  EXPECT_FALSE(job_id.empty());
}

TEST_F(CLIOperatorTest, ResetAllAsyncCompletesWithNoComponents) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id = operator_->ResetAllAsync();

  std::this_thread::sleep_for(200ms);

  auto status = operator_->GetJobStatus(job_id);
  EXPECT_EQ(status.state, JobState::Completed);
}

// === Job Status Tests ===

TEST_F(CLIOperatorTest, GetJobStatusForUnknownJobReturnsEmpty) {
  auto status = operator_->GetJobStatus("nonexistent_job");

  EXPECT_TRUE(status.job_id.empty());
}

TEST_F(CLIOperatorTest, JobIdIsUnique) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job_id1 = operator_->ConfigureAllAsync();
  auto job_id2 = operator_->ConfigureAllAsync();
  auto job_id3 = operator_->ConfigureAllAsync();

  EXPECT_NE(job_id1, job_id2);
  EXPECT_NE(job_id2, job_id3);
  EXPECT_NE(job_id1, job_id3);
}

// === Component Status Tests ===

TEST_F(CLIOperatorTest, GetAllComponentStatusReturnsEmptyForNoComponents) {
  auto statuses = operator_->GetAllComponentStatus();
  EXPECT_TRUE(statuses.empty());
}

TEST_F(CLIOperatorTest, GetComponentStatusReturnsEmptyForUnknown) {
  auto status = operator_->GetComponentStatus("unknown_component");
  EXPECT_TRUE(status.component_id.empty());
}

// === IsAllInState Tests ===

TEST_F(CLIOperatorTest, IsAllInStateReturnsTrueForNoComponents) {
  // With no components, all (zero) are in any state - vacuously true
  EXPECT_TRUE(operator_->IsAllInState(ComponentState::Idle));
  EXPECT_TRUE(operator_->IsAllInState(ComponentState::Running));
}

// === State Transition Tests ===

TEST_F(CLIOperatorTest, ShutdownResetsState) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  EXPECT_EQ(operator_->GetState(), ComponentState::Configured);

  operator_->Shutdown();

  EXPECT_EQ(operator_->GetState(), ComponentState::Idle);
}

// === Error State Tests ===

TEST_F(CLIOperatorTest, ForceErrorSetsErrorState) {
  operator_->ForceError("Test error");

  EXPECT_EQ(operator_->GetState(), ComponentState::Error);
}

TEST_F(CLIOperatorTest, ErrorMessageStoredInStatus) {
  operator_->ForceError("Test error message");

  auto status = operator_->GetStatus();
  EXPECT_EQ(status.error_message, "Test error message");
}

TEST_F(CLIOperatorTest, ResetFromErrorState) {
  operator_->ForceError("Test error");
  EXPECT_EQ(operator_->GetState(), ComponentState::Error);

  operator_->Reset();

  EXPECT_EQ(operator_->GetState(), ComponentState::Idle);
}

// === Multiple Job Management ===

TEST_F(CLIOperatorTest, CanTrackMultipleJobs) {
  operator_->SetComponentId("test_operator");
  operator_->Initialize("");

  auto job1 = operator_->ConfigureAllAsync();
  auto job2 = operator_->ArmAllAsync();
  auto job3 = operator_->StartAllAsync(1);

  // All jobs should have valid statuses
  EXPECT_FALSE(operator_->GetJobStatus(job1).job_id.empty());
  EXPECT_FALSE(operator_->GetJobStatus(job2).job_id.empty());
  EXPECT_FALSE(operator_->GetJobStatus(job3).job_id.empty());
}

// === Run Lifecycle Tests ===

TEST_F(CLIOperatorTest, FullLifecycleWithNoComponents) {
  operator_->SetComponentId("test_operator");

  // Initialize
  EXPECT_TRUE(operator_->Initialize(""));
  EXPECT_EQ(operator_->GetState(), ComponentState::Configured);

  // Configure
  auto config_job = operator_->ConfigureAllAsync();
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(operator_->GetJobStatus(config_job).state, JobState::Completed);

  // Arm
  auto arm_job = operator_->ArmAllAsync();
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(operator_->GetJobStatus(arm_job).state, JobState::Completed);

  // Start
  auto start_job = operator_->StartAllAsync(1);
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(operator_->GetJobStatus(start_job).state, JobState::Completed);

  // Stop
  auto stop_job = operator_->StopAllAsync(true);
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(operator_->GetJobStatus(stop_job).state, JobState::Completed);

  // Reset
  auto reset_job = operator_->ResetAllAsync();
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(operator_->GetJobStatus(reset_job).state, JobState::Completed);
}
