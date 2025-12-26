/**
 * @file test_component_state.cpp
 * @brief Unit tests for ComponentState
 */

#include <gtest/gtest.h>

#include "ComponentState.hpp"

using namespace DELILA::Net;

TEST(ComponentStateTest, StateToStringConversion)
{
    EXPECT_EQ(ComponentStateToString(ComponentState::Loaded), "Loaded");
    EXPECT_EQ(ComponentStateToString(ComponentState::Configured), "Configured");
    EXPECT_EQ(ComponentStateToString(ComponentState::Armed), "Armed");
    EXPECT_EQ(ComponentStateToString(ComponentState::Running), "Running");
    EXPECT_EQ(ComponentStateToString(ComponentState::Paused), "Paused");
    EXPECT_EQ(ComponentStateToString(ComponentState::Error), "Error");
}

TEST(ComponentStateTest, ValidTransitionLoadedToConfigure)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Loaded, ComponentState::Configured));
}

TEST(ComponentStateTest, ValidTransitionConfiguredToArmed)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Armed));
}

TEST(ComponentStateTest, ValidTransitionArmedToRunning)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Running));
}

TEST(ComponentStateTest, ValidTransitionRunningToPaused)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Paused));
}

TEST(ComponentStateTest, ValidTransitionPausedToRunning)
{
    EXPECT_TRUE(IsValidTransition(ComponentState::Paused, ComponentState::Running));
}

TEST(ComponentStateTest, ValidTransitionToLoaded)
{
    // Any state can transition to Loaded (reset/stop)
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Loaded));
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Loaded));
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Loaded));
    EXPECT_TRUE(IsValidTransition(ComponentState::Paused, ComponentState::Loaded));
    EXPECT_TRUE(IsValidTransition(ComponentState::Error, ComponentState::Loaded));
}

TEST(ComponentStateTest, ValidTransitionToError)
{
    // Any state can transition to Error
    EXPECT_TRUE(IsValidTransition(ComponentState::Loaded, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Paused, ComponentState::Error));
}

TEST(ComponentStateTest, InvalidTransitionFromLoaded)
{
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Armed));
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Running));
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Paused));
}

TEST(ComponentStateTest, InvalidTransitionFromArmed)
{
    // Cannot go directly from Armed to Configured or Paused
    EXPECT_FALSE(IsValidTransition(ComponentState::Armed, ComponentState::Configured));
    EXPECT_FALSE(IsValidTransition(ComponentState::Armed, ComponentState::Paused));
}

TEST(ComponentStateTest, InvalidTransitionFromRunning)
{
    // Cannot go directly from Running to Configured or Armed
    EXPECT_FALSE(IsValidTransition(ComponentState::Running, ComponentState::Configured));
    EXPECT_FALSE(IsValidTransition(ComponentState::Running, ComponentState::Armed));
}

TEST(ComponentStateTest, SameStateTransition)
{
    // Same state is not a valid transition
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Loaded));
    EXPECT_FALSE(IsValidTransition(ComponentState::Running, ComponentState::Running));
}
