/**
 * @file test_heartbeat_manager.cpp
 * @brief Unit tests for HeartbeatManager
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "HeartbeatManager.hpp"

using namespace DELILA::Net;
using namespace std::chrono_literals;

TEST(HeartbeatManagerTest, InitialStateNotDue)
{
    HeartbeatManager manager(100ms);
    // Just created, not due yet
    EXPECT_FALSE(manager.IsDue());
}

TEST(HeartbeatManagerTest, IsDueAfterInterval)
{
    HeartbeatManager manager(10ms);  // Short interval for testing

    // Wait for interval to pass
    std::this_thread::sleep_for(15ms);

    EXPECT_TRUE(manager.IsDue());
}

TEST(HeartbeatManagerTest, MarkSentResetsTimer)
{
    HeartbeatManager manager(10ms);

    // Wait for interval
    std::this_thread::sleep_for(15ms);
    EXPECT_TRUE(manager.IsDue());

    // Mark as sent
    manager.MarkSent();

    // Should not be due immediately after marking
    EXPECT_FALSE(manager.IsDue());
}

TEST(HeartbeatManagerTest, ConfigurableInterval)
{
    HeartbeatManager manager(50ms);

    // After 30ms, should not be due
    std::this_thread::sleep_for(30ms);
    EXPECT_FALSE(manager.IsDue());

    // After total 60ms, should be due
    std::this_thread::sleep_for(30ms);
    EXPECT_TRUE(manager.IsDue());
}

TEST(HeartbeatManagerTest, SetIntervalChangesInterval)
{
    HeartbeatManager manager(100ms);

    // Change to shorter interval
    manager.SetInterval(10ms);

    std::this_thread::sleep_for(15ms);
    EXPECT_TRUE(manager.IsDue());
}

TEST(HeartbeatManagerTest, GetIntervalReturnsCurrentInterval)
{
    HeartbeatManager manager(100ms);
    EXPECT_EQ(manager.GetInterval(), 100ms);

    manager.SetInterval(50ms);
    EXPECT_EQ(manager.GetInterval(), 50ms);
}

TEST(HeartbeatManagerTest, MultipleHeartbeatCycles)
{
    HeartbeatManager manager(10ms);

    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(15ms);
        EXPECT_TRUE(manager.IsDue()) << "Cycle " << i;
        manager.MarkSent();
        EXPECT_FALSE(manager.IsDue()) << "After MarkSent cycle " << i;
    }
}

TEST(HeartbeatManagerTest, DefaultInterval)
{
    HeartbeatManager manager;  // Default constructor
    EXPECT_EQ(manager.GetInterval(), 100ms);
}
