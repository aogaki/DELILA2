/**
 * @file test_eos_tracker.cpp
 * @brief Unit tests for EOSTracker
 */

#include <gtest/gtest.h>

#include "EOSTracker.hpp"

using namespace DELILA::Net;

TEST(EOSTrackerTest, RegisterSourceAddsToExpected)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");

    EXPECT_EQ(tracker.GetExpectedCount(), 2);
}

TEST(EOSTrackerTest, UnregisterSourceRemovesFromExpected)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");
    tracker.UnregisterSource("source1");

    EXPECT_EQ(tracker.GetExpectedCount(), 1);
}

TEST(EOSTrackerTest, ReceiveEOSMarksSourceComplete)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.ReceiveEOS("source1");

    EXPECT_EQ(tracker.GetReceivedCount(), 1);
}

TEST(EOSTrackerTest, UnregisteredSourceEOSIsIgnored)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.ReceiveEOS("unknown_source");

    EXPECT_EQ(tracker.GetReceivedCount(), 0);
}

TEST(EOSTrackerTest, AllReceivedWhenAllSourcesComplete)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");
    tracker.RegisterSource("source3");

    EXPECT_FALSE(tracker.AllReceived());

    tracker.ReceiveEOS("source1");
    EXPECT_FALSE(tracker.AllReceived());

    tracker.ReceiveEOS("source2");
    EXPECT_FALSE(tracker.AllReceived());

    tracker.ReceiveEOS("source3");
    EXPECT_TRUE(tracker.AllReceived());
}

TEST(EOSTrackerTest, NotAllReceivedWhenSomeSourcesPending)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");

    tracker.ReceiveEOS("source1");

    EXPECT_FALSE(tracker.AllReceived());
}

TEST(EOSTrackerTest, ResetClearsAllState)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");
    tracker.ReceiveEOS("source1");

    tracker.Reset();

    EXPECT_EQ(tracker.GetExpectedCount(), 0);
    EXPECT_EQ(tracker.GetReceivedCount(), 0);
    EXPECT_TRUE(tracker.GetPendingSources().empty());
}

TEST(EOSTrackerTest, GetPendingSourcesReturnsCorrectList)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");
    tracker.RegisterSource("source3");

    tracker.ReceiveEOS("source2");

    auto pending = tracker.GetPendingSources();

    EXPECT_EQ(pending.size(), 2);
    EXPECT_TRUE(std::find(pending.begin(), pending.end(), "source1") !=
                pending.end());
    EXPECT_TRUE(std::find(pending.begin(), pending.end(), "source3") !=
                pending.end());
    EXPECT_TRUE(std::find(pending.begin(), pending.end(), "source2") ==
                pending.end());
}

TEST(EOSTrackerTest, AllReceivedTrueWhenNoSources)
{
    EOSTracker tracker;

    // No sources registered = all received (vacuously true)
    EXPECT_TRUE(tracker.AllReceived());
}

TEST(EOSTrackerTest, DuplicateEOSIsIgnored)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.ReceiveEOS("source1");
    tracker.ReceiveEOS("source1");  // Duplicate

    EXPECT_EQ(tracker.GetReceivedCount(), 1);
}

TEST(EOSTrackerTest, HasReceivedEOS)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");
    tracker.RegisterSource("source2");

    EXPECT_FALSE(tracker.HasReceivedEOS("source1"));
    EXPECT_FALSE(tracker.HasReceivedEOS("source2"));

    tracker.ReceiveEOS("source1");

    EXPECT_TRUE(tracker.HasReceivedEOS("source1"));
    EXPECT_FALSE(tracker.HasReceivedEOS("source2"));
}

TEST(EOSTrackerTest, IsRegistered)
{
    EOSTracker tracker;

    tracker.RegisterSource("source1");

    EXPECT_TRUE(tracker.IsRegistered("source1"));
    EXPECT_FALSE(tracker.IsRegistered("source2"));
}
