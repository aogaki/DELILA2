/**
 * @file test_heartbeat_monitor.cpp
 * @brief Unit tests for HeartbeatMonitor
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "HeartbeatMonitor.hpp"

using namespace DELILA::Net;
using namespace std::chrono_literals;

TEST(HeartbeatMonitorTest, InitialStateNotTimedOut)
{
    HeartbeatMonitor monitor(100ms);

    // Unknown source should not be timed out (not registered yet)
    EXPECT_FALSE(monitor.IsTimedOut("source1"));
}

TEST(HeartbeatMonitorTest, UpdateRegistersSource)
{
    HeartbeatMonitor monitor(100ms);

    monitor.Update("source1");

    // Source should exist and not be timed out
    EXPECT_FALSE(monitor.IsTimedOut("source1"));
}

TEST(HeartbeatMonitorTest, TimesOutAfterThreshold)
{
    HeartbeatMonitor monitor(10ms);  // Short timeout for testing

    monitor.Update("source1");
    EXPECT_FALSE(monitor.IsTimedOut("source1"));

    // Wait for timeout
    std::this_thread::sleep_for(15ms);

    EXPECT_TRUE(monitor.IsTimedOut("source1"));
}

TEST(HeartbeatMonitorTest, UpdateResetsTimer)
{
    HeartbeatMonitor monitor(20ms);

    monitor.Update("source1");

    // Wait partial timeout
    std::this_thread::sleep_for(15ms);
    EXPECT_FALSE(monitor.IsTimedOut("source1"));

    // Update resets timer
    monitor.Update("source1");

    // Wait another partial timeout
    std::this_thread::sleep_for(15ms);
    EXPECT_FALSE(monitor.IsTimedOut("source1"));  // Still not timed out

    // Wait for full timeout
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(monitor.IsTimedOut("source1"));
}

TEST(HeartbeatMonitorTest, MultipleSourcesTrackedIndependently)
{
    HeartbeatMonitor monitor(20ms);

    monitor.Update("source1");
    monitor.Update("source2");

    std::this_thread::sleep_for(15ms);

    // Update only source1
    monitor.Update("source1");

    std::this_thread::sleep_for(10ms);

    // source1 should be OK, source2 should be timed out
    EXPECT_FALSE(monitor.IsTimedOut("source1"));
    EXPECT_TRUE(monitor.IsTimedOut("source2"));
}

TEST(HeartbeatMonitorTest, GetTimedOutSourcesReturnsCorrectList)
{
    HeartbeatMonitor monitor(10ms);

    monitor.Update("source1");
    monitor.Update("source2");
    monitor.Update("source3");

    std::this_thread::sleep_for(15ms);

    // Update only source2
    monitor.Update("source2");

    auto timedOut = monitor.GetTimedOutSources();

    EXPECT_EQ(timedOut.size(), 2);
    EXPECT_TRUE(std::find(timedOut.begin(), timedOut.end(), "source1") !=
                timedOut.end());
    EXPECT_TRUE(std::find(timedOut.begin(), timedOut.end(), "source3") !=
                timedOut.end());
    EXPECT_TRUE(std::find(timedOut.begin(), timedOut.end(), "source2") ==
                timedOut.end());
}

TEST(HeartbeatMonitorTest, RemoveSourceStopsTracking)
{
    HeartbeatMonitor monitor(100ms);

    monitor.Update("source1");
    monitor.Remove("source1");

    // After removal, should not be tracked
    EXPECT_FALSE(monitor.IsTimedOut("source1"));
    EXPECT_TRUE(monitor.GetTimedOutSources().empty());
}

TEST(HeartbeatMonitorTest, ClearRemovesAllSources)
{
    HeartbeatMonitor monitor(10ms);

    monitor.Update("source1");
    monitor.Update("source2");

    std::this_thread::sleep_for(15ms);

    monitor.Clear();

    EXPECT_TRUE(monitor.GetTimedOutSources().empty());
    EXPECT_FALSE(monitor.IsTimedOut("source1"));
    EXPECT_FALSE(monitor.IsTimedOut("source2"));
}

TEST(HeartbeatMonitorTest, DefaultTimeout)
{
    HeartbeatMonitor monitor;  // Default constructor
    EXPECT_EQ(monitor.GetTimeout(), 6000ms);
}

TEST(HeartbeatMonitorTest, GetSourceCount)
{
    HeartbeatMonitor monitor(100ms);

    EXPECT_EQ(monitor.GetSourceCount(), 0);

    monitor.Update("source1");
    EXPECT_EQ(monitor.GetSourceCount(), 1);

    monitor.Update("source2");
    EXPECT_EQ(monitor.GetSourceCount(), 2);

    monitor.Remove("source1");
    EXPECT_EQ(monitor.GetSourceCount(), 1);
}
