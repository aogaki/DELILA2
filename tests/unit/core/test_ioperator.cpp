/**
 * @file test_ioperator.cpp
 * @brief Unit tests for IOperator interface
 */

#include <delila/core/IOperator.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace DELILA {
namespace test {

/**
 * @brief Mock implementation of IOperator for testing
 */
class MockOperator : public IOperator {
public:
  // IComponent methods
  MOCK_METHOD(bool, Initialize, (const std::string &config_path), (override));
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(ComponentState, GetState, (), (const, override));
  MOCK_METHOD(std::string, GetComponentId, (), (const, override));
  MOCK_METHOD(ComponentStatus, GetStatus, (), (const, override));

  // IOperator methods - async commands
  MOCK_METHOD(std::string, ConfigureAllAsync, (), (override));
  MOCK_METHOD(std::string, ArmAllAsync, (), (override));
  MOCK_METHOD(std::string, StartAllAsync, (uint32_t run_number), (override));
  MOCK_METHOD(std::string, StopAllAsync, (bool graceful), (override));
  MOCK_METHOD(std::string, ResetAllAsync, (), (override));

  // IOperator methods - job status
  MOCK_METHOD(JobStatus, GetJobStatus, (const std::string &job_id),
              (const, override));

  // IOperator methods - component status
  MOCK_METHOD(std::vector<ComponentStatus>, GetAllComponentStatus, (),
              (const, override));
  MOCK_METHOD(ComponentStatus, GetComponentStatus,
              (const std::string &component_id), (const, override));

  // IOperator methods - component management
  MOCK_METHOD(std::vector<std::string>, GetComponentIds, (), (const, override));
  MOCK_METHOD(bool, IsAllInState, (ComponentState state), (const, override));

protected:
  MOCK_METHOD(bool, OnConfigure, (const nlohmann::json &config), (override));
  MOCK_METHOD(bool, OnArm, (), (override));
  MOCK_METHOD(bool, OnStart, (uint32_t run_number), (override));
  MOCK_METHOD(bool, OnStop, (bool graceful), (override));
  MOCK_METHOD(void, OnReset, (), (override));
};

class IOperatorTest : public ::testing::Test {
protected:
  void SetUp() override { operator_ = std::make_unique<MockOperator>(); }

  std::unique_ptr<MockOperator> operator_;
};

TEST_F(IOperatorTest, InheritsFromIComponent) {
  IComponent *base = operator_.get();
  EXPECT_NE(base, nullptr);
}

TEST_F(IOperatorTest, ConfigureAllAsyncReturnsJobId) {
  std::string job_id = "job_001";

  EXPECT_CALL(*operator_, ConfigureAllAsync())
      .WillOnce(::testing::Return(job_id));

  auto result = operator_->ConfigureAllAsync();

  EXPECT_EQ(result, job_id);
  EXPECT_FALSE(result.empty());
}

TEST_F(IOperatorTest, ArmAllAsyncReturnsJobId) {
  std::string job_id = "job_002";

  EXPECT_CALL(*operator_, ArmAllAsync()).WillOnce(::testing::Return(job_id));

  auto result = operator_->ArmAllAsync();

  EXPECT_EQ(result, job_id);
}

TEST_F(IOperatorTest, StartAllAsyncWithRunNumber) {
  std::string job_id = "job_003";
  uint32_t run_number = 100;

  EXPECT_CALL(*operator_, StartAllAsync(run_number))
      .WillOnce(::testing::Return(job_id));

  auto result = operator_->StartAllAsync(run_number);

  EXPECT_EQ(result, job_id);
}

TEST_F(IOperatorTest, StopAllAsyncGraceful) {
  std::string job_id = "job_004";

  EXPECT_CALL(*operator_, StopAllAsync(true))
      .WillOnce(::testing::Return(job_id));

  auto result = operator_->StopAllAsync(true);

  EXPECT_EQ(result, job_id);
}

TEST_F(IOperatorTest, StopAllAsyncEmergency) {
  std::string job_id = "job_005";

  EXPECT_CALL(*operator_, StopAllAsync(false))
      .WillOnce(::testing::Return(job_id));

  auto result = operator_->StopAllAsync(false);

  EXPECT_EQ(result, job_id);
}

TEST_F(IOperatorTest, ResetAllAsyncReturnsJobId) {
  std::string job_id = "job_006";

  EXPECT_CALL(*operator_, ResetAllAsync()).WillOnce(::testing::Return(job_id));

  auto result = operator_->ResetAllAsync();

  EXPECT_EQ(result, job_id);
}

TEST_F(IOperatorTest, GetJobStatusReturnsCorrectState) {
  std::string job_id = "job_001";
  JobStatus status;
  status.job_id = job_id;
  status.state = JobState::Completed;

  EXPECT_CALL(*operator_, GetJobStatus(job_id))
      .WillOnce(::testing::Return(status));

  auto result = operator_->GetJobStatus(job_id);

  EXPECT_EQ(result.job_id, job_id);
  EXPECT_EQ(result.state, JobState::Completed);
}

TEST_F(IOperatorTest, GetAllComponentStatusReturnsMultiple) {
  std::vector<ComponentStatus> statuses;
  ComponentStatus s1;
  s1.component_id = "source_01";
  s1.state = ComponentState::Running;
  ComponentStatus s2;
  s2.component_id = "writer_01";
  s2.state = ComponentState::Running;
  statuses.push_back(s1);
  statuses.push_back(s2);

  EXPECT_CALL(*operator_, GetAllComponentStatus())
      .WillOnce(::testing::Return(statuses));

  auto result = operator_->GetAllComponentStatus();

  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].component_id, "source_01");
  EXPECT_EQ(result[1].component_id, "writer_01");
}

TEST_F(IOperatorTest, GetComponentStatusByIdReturnsCorrect) {
  std::string component_id = "source_01";
  ComponentStatus status;
  status.component_id = component_id;
  status.state = ComponentState::Configured;

  EXPECT_CALL(*operator_, GetComponentStatus(component_id))
      .WillOnce(::testing::Return(status));

  auto result = operator_->GetComponentStatus(component_id);

  EXPECT_EQ(result.component_id, component_id);
  EXPECT_EQ(result.state, ComponentState::Configured);
}

TEST_F(IOperatorTest, GetComponentIdsReturnsList) {
  std::vector<std::string> ids = {"source_01", "source_02", "writer_01"};

  EXPECT_CALL(*operator_, GetComponentIds()).WillOnce(::testing::Return(ids));

  auto result = operator_->GetComponentIds();

  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], "source_01");
}

TEST_F(IOperatorTest, IsAllInStateChecksAllComponents) {
  EXPECT_CALL(*operator_, IsAllInState(ComponentState::Armed))
      .WillOnce(::testing::Return(true));

  EXPECT_TRUE(operator_->IsAllInState(ComponentState::Armed));
}

TEST_F(IOperatorTest, IsAllInStateReturnsFalseWhenNotAll) {
  EXPECT_CALL(*operator_, IsAllInState(ComponentState::Running))
      .WillOnce(::testing::Return(false));

  EXPECT_FALSE(operator_->IsAllInState(ComponentState::Running));
}

TEST_F(IOperatorTest, JobStatePendingState) {
  JobStatus status;
  status.job_id = "job_007";
  status.state = JobState::Pending;

  EXPECT_CALL(*operator_, GetJobStatus("job_007"))
      .WillOnce(::testing::Return(status));

  auto result = operator_->GetJobStatus("job_007");

  EXPECT_EQ(result.state, JobState::Pending);
}

TEST_F(IOperatorTest, JobStateRunningState) {
  JobStatus status;
  status.job_id = "job_008";
  status.state = JobState::Running;

  EXPECT_CALL(*operator_, GetJobStatus("job_008"))
      .WillOnce(::testing::Return(status));

  auto result = operator_->GetJobStatus("job_008");

  EXPECT_EQ(result.state, JobState::Running);
}

TEST_F(IOperatorTest, JobStateFailedState) {
  JobStatus status;
  status.job_id = "job_009";
  status.state = JobState::Failed;
  status.error_message = "Connection timeout";

  EXPECT_CALL(*operator_, GetJobStatus("job_009"))
      .WillOnce(::testing::Return(status));

  auto result = operator_->GetJobStatus("job_009");

  EXPECT_EQ(result.state, JobState::Failed);
  EXPECT_EQ(result.error_message, "Connection timeout");
}

} // namespace test
} // namespace DELILA
