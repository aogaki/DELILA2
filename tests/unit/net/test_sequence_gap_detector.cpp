/**
 * @file test_sequence_gap_detector.cpp
 * @brief Unit tests for SequenceGapDetector
 *
 * TDD Phase: RED - Write tests first
 */

#include <gtest/gtest.h>

#include "SequenceGapDetector.hpp"

using namespace DELILA::Net;

// Test: Initial state has no expected sequence
TEST(SequenceGapDetectorTest, InitialStateHasNoExpectedSequence)
{
    SequenceGapDetector detector;
    EXPECT_FALSE(detector.HasExpectedSequence());
    EXPECT_EQ(detector.GetGapCount(), 0);
}

// Test: First message sets expected sequence
TEST(SequenceGapDetectorTest, FirstMessageSetsExpectedSequence)
{
    SequenceGapDetector detector;

    auto result = detector.Check(0);
    EXPECT_EQ(result, SequenceGapDetector::Result::Ok);
    EXPECT_TRUE(detector.HasExpectedSequence());
}

// Test: Consecutive sequences return Ok
TEST(SequenceGapDetectorTest, ConsecutiveSequencesReturnOk)
{
    SequenceGapDetector detector;

    EXPECT_EQ(detector.Check(0), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(1), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(2), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(3), SequenceGapDetector::Result::Ok);

    EXPECT_EQ(detector.GetGapCount(), 0);
}

// Test: Detects gap of one
TEST(SequenceGapDetectorTest, DetectsGapOfOne)
{
    SequenceGapDetector detector;

    EXPECT_EQ(detector.Check(0), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(1), SequenceGapDetector::Result::Ok);
    // Skip 2, receive 3
    EXPECT_EQ(detector.Check(3), SequenceGapDetector::Result::Gap);

    EXPECT_EQ(detector.GetGapCount(), 1);

    auto gap = detector.GetLastGap();
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->expected, 2);
    EXPECT_EQ(gap->received, 3);
    EXPECT_EQ(gap->dropped_count, 1);
}

// Test: Detects gap of multiple
TEST(SequenceGapDetectorTest, DetectsGapOfMultiple)
{
    SequenceGapDetector detector;

    EXPECT_EQ(detector.Check(0), SequenceGapDetector::Result::Ok);
    // Skip 1, 2, 3, 4, receive 5
    EXPECT_EQ(detector.Check(5), SequenceGapDetector::Result::Gap);

    auto gap = detector.GetLastGap();
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->expected, 1);
    EXPECT_EQ(gap->received, 5);
    EXPECT_EQ(gap->dropped_count, 4);
}

// Test: Reset clears state
TEST(SequenceGapDetectorTest, ResetClearsState)
{
    SequenceGapDetector detector;

    detector.Check(0);
    detector.Check(5);  // Gap detected
    EXPECT_EQ(detector.GetGapCount(), 1);

    detector.Reset();

    EXPECT_FALSE(detector.HasExpectedSequence());
    EXPECT_EQ(detector.GetGapCount(), 0);
    EXPECT_FALSE(detector.GetLastGap().has_value());
}

// Test: Backwards sequence is error
TEST(SequenceGapDetectorTest, BackwardsSequenceIsError)
{
    SequenceGapDetector detector;

    EXPECT_EQ(detector.Check(5), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(6), SequenceGapDetector::Result::Ok);
    // Receive older sequence
    EXPECT_EQ(detector.Check(3), SequenceGapDetector::Result::BackwardsSequence);
}

// Test: Gap count increments on each gap
TEST(SequenceGapDetectorTest, GapCountIncrementsOnEachGap)
{
    SequenceGapDetector detector;

    detector.Check(0);
    detector.Check(2);  // Gap 1
    detector.Check(5);  // Gap 2
    detector.Check(10); // Gap 3

    EXPECT_EQ(detector.GetGapCount(), 3);
}

// Test: GetLastGap returns correct info for most recent gap
TEST(SequenceGapDetectorTest, GetLastGapReturnsCorrectInfo)
{
    SequenceGapDetector detector;

    detector.Check(0);
    detector.Check(3);  // First gap: expected 1, received 3
    detector.Check(7);  // Second gap: expected 4, received 7

    auto gap = detector.GetLastGap();
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->expected, 4);
    EXPECT_EQ(gap->received, 7);
    EXPECT_EQ(gap->dropped_count, 3);
}

// Test: First message can start from non-zero sequence
TEST(SequenceGapDetectorTest, FirstMessageCanStartFromNonZero)
{
    SequenceGapDetector detector;

    EXPECT_EQ(detector.Check(100), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(101), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(102), SequenceGapDetector::Result::Ok);

    EXPECT_EQ(detector.GetGapCount(), 0);
}

// Test: After gap, continue checking from received sequence
TEST(SequenceGapDetectorTest, ContinuesAfterGap)
{
    SequenceGapDetector detector;

    detector.Check(0);
    detector.Check(5);  // Gap, now expects 6

    EXPECT_EQ(detector.Check(6), SequenceGapDetector::Result::Ok);
    EXPECT_EQ(detector.Check(7), SequenceGapDetector::Result::Ok);
}
