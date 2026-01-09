/**
 * @file test_component_state.cpp
 * @brief Unit tests for ComponentState
 */

#include <gtest/gtest.h>

#include <delila/core/ComponentState.hpp>

using namespace DELILA;

TEST(ComponentStateTest, StateToStringConversion)
{
    EXPECT_EQ(ComponentStateToString(ComponentState::Idle), "Idle");
    EXPECT_EQ(ComponentStateToString(ComponentState::Configuring), "Configuring");
    EXPECT_EQ(ComponentStateToString(ComponentState::Configured), "Configured");
    EXPECT_EQ(ComponentStateToString(ComponentState::Arming), "Arming");
    EXPECT_EQ(ComponentStateToString(ComponentState::Armed), "Armed");
    EXPECT_EQ(ComponentStateToString(ComponentState::Starting), "Starting");
    EXPECT_EQ(ComponentStateToString(ComponentState::Running), "Running");
    EXPECT_EQ(ComponentStateToString(ComponentState::Stopping), "Stopping");
    EXPECT_EQ(ComponentStateToString(ComponentState::Error), "Error");
}

TEST(ComponentStateTest, ValidTransitionIdleToConfiguring)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Idle, ComponentState::Configuring));
}

TEST(ComponentStateTest, ValidTransitionConfiguringToConfigured)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Configuring, ComponentState::Configured));
}

TEST(ComponentStateTest, ValidTransitionConfiguredToArming)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Arming));
}

TEST(ComponentStateTest, ValidTransitionArmingToArmed)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Arming, ComponentState::Armed));
}

TEST(ComponentStateTest, ValidTransitionArmedToStarting)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Starting));
}

TEST(ComponentStateTest, ValidTransitionStartingToRunning)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Starting, ComponentState::Running));
}

TEST(ComponentStateTest, ValidTransitionRunningToStopping)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Stopping));
}

TEST(ComponentStateTest, ValidTransitionStoppingToConfigured)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Stopping, ComponentState::Configured));
}

TEST(ComponentStateTest, ValidTransitionToIdle)
{
    // Any state can transition to Idle (reset/stop)
    EXPECT_TRUE(IsValidTransition(ComponentState::Configuring, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Arming, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Starting, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Stopping, ComponentState::Idle));
    EXPECT_TRUE(IsValidTransition(ComponentState::Error, ComponentState::Idle));
}

TEST(ComponentStateTest, ValidTransitionToError)
{
    // Any state can transition to Error
    EXPECT_TRUE(IsValidTransition(ComponentState::Idle, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configuring, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Arming, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Starting, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Stopping, ComponentState::Error));
}

TEST(ComponentStateTest, InvalidTransitionFromIdle)
{
    EXPECT_FALSE(IsValidTransition(ComponentState::Idle, ComponentState::Configured));
    EXPECT_FALSE(IsValidTransition(ComponentState::Idle, ComponentState::Armed));
    EXPECT_FALSE(IsValidTransition(ComponentState::Idle, ComponentState::Running));
}

TEST(ComponentStateTest, InvalidTransitionFromConfigured)
{
    // Cannot skip to Armed or Running directly
    EXPECT_FALSE(IsValidTransition(ComponentState::Configured, ComponentState::Armed));
    EXPECT_FALSE(IsValidTransition(ComponentState::Configured, ComponentState::Running));
}

TEST(ComponentStateTest, InvalidTransitionFromArmed)
{
    // Cannot go directly from Armed to Running or back to Configured
    EXPECT_FALSE(IsValidTransition(ComponentState::Armed, ComponentState::Running));
    EXPECT_FALSE(IsValidTransition(ComponentState::Armed, ComponentState::Configured));
}

TEST(ComponentStateTest, InvalidTransitionFromRunning)
{
    // Cannot skip Stopping state
    EXPECT_FALSE(IsValidTransition(ComponentState::Running, ComponentState::Configured));
    EXPECT_FALSE(IsValidTransition(ComponentState::Running, ComponentState::Armed));
}

TEST(ComponentStateTest, SameStateTransition)
{
    // Same state is not a valid transition
    EXPECT_FALSE(IsValidTransition(ComponentState::Idle, ComponentState::Idle));
    EXPECT_FALSE(IsValidTransition(ComponentState::Configuring, ComponentState::Configuring));
    EXPECT_FALSE(IsValidTransition(ComponentState::Running, ComponentState::Running));
}
