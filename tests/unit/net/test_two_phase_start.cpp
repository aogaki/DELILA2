/**
 * @file test_two_phase_start.cpp
 * @brief Unit tests for TwoPhaseStartManager
 */

#include <gtest/gtest.h>

#include "TwoPhaseStartManager.hpp"

using namespace DELILA::Net;

class TwoPhaseStartTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        manager_ = std::make_unique<TwoPhaseStartManager>();
    }

    std::unique_ptr<TwoPhaseStartManager> manager_;
};

TEST_F(TwoPhaseStartTest, InitialStateIsLoaded)
{
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, ConfigureTransitionsToConfigured)
{
    auto result = manager_->Configure();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Configured);
}

TEST_F(TwoPhaseStartTest, ArmFromLoadedFails)
{
    auto result = manager_->Arm();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::InvalidState);
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, ArmFromConfiguredSucceeds)
{
    manager_->Configure();
    auto result = manager_->Arm();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Armed);
}

TEST_F(TwoPhaseStartTest, TriggerFromArmedSucceeds)
{
    manager_->Configure();
    manager_->Arm();
    auto result = manager_->Trigger();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Running);
}

TEST_F(TwoPhaseStartTest, TriggerFromConfiguredFails)
{
    manager_->Configure();
    auto result = manager_->Trigger();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::NotArmed);
    EXPECT_EQ(manager_->GetState(), ComponentState::Configured);
}

TEST_F(TwoPhaseStartTest, TriggerFromLoadedFails)
{
    auto result = manager_->Trigger();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::NotArmed);
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, StopFromRunningSucceeds)
{
    manager_->Configure();
    manager_->Arm();
    manager_->Trigger();

    auto result = manager_->Stop();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, StopFromArmedSucceeds)
{
    manager_->Configure();
    manager_->Arm();

    auto result = manager_->Stop();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, ResetFromAnyStateSucceeds)
{
    manager_->Configure();
    manager_->Arm();
    manager_->Trigger();

    auto result = manager_->Reset();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, DoubleArmReturnsAlreadyArmed)
{
    manager_->Configure();
    manager_->Arm();

    auto result = manager_->Arm();
    EXPECT_EQ(result, TwoPhaseStartManager::Result::AlreadyArmed);
    EXPECT_EQ(manager_->GetState(), ComponentState::Armed);
}

TEST_F(TwoPhaseStartTest, CompleteWorkflow)
{
    // Full 2-phase start workflow
    EXPECT_EQ(manager_->Configure(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Configured);

    EXPECT_EQ(manager_->Arm(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Armed);

    EXPECT_EQ(manager_->Trigger(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Running);

    EXPECT_EQ(manager_->Stop(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_->GetState(), ComponentState::Loaded);
}

TEST_F(TwoPhaseStartTest, IsArmed)
{
    EXPECT_FALSE(manager_->IsArmed());

    manager_->Configure();
    EXPECT_FALSE(manager_->IsArmed());

    manager_->Arm();
    EXPECT_TRUE(manager_->IsArmed());

    manager_->Trigger();
    EXPECT_FALSE(manager_->IsArmed());
}

TEST_F(TwoPhaseStartTest, IsRunning)
{
    EXPECT_FALSE(manager_->IsRunning());

    manager_->Configure();
    manager_->Arm();
    EXPECT_FALSE(manager_->IsRunning());

    manager_->Trigger();
    EXPECT_TRUE(manager_->IsRunning());

    manager_->Stop();
    EXPECT_FALSE(manager_->IsRunning());
}
