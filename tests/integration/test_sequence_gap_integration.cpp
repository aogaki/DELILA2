/**
 * @file test_sequence_gap_integration.cpp
 * @brief Integration tests for sequence gap detection in data flow
 *
 * Tests the complete flow: DataProcessor (sender) -> SequenceGapDetector (receiver)
 */

#include <gtest/gtest.h>

#include "DataProcessor.hpp"
#include "SequenceGapDetector.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

class SequenceGapIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sender_ = std::make_unique<DataProcessor>();
        receiver_ = std::make_unique<DataProcessor>();
        gap_detector_ = std::make_unique<SequenceGapDetector>();
    }

    std::unique_ptr<DataProcessor> sender_;
    std::unique_ptr<DataProcessor> receiver_;
    std::unique_ptr<SequenceGapDetector> gap_detector_;

    // Helper to create test events
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>
    CreateTestEvents(size_t count)
    {
        auto events =
            std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<MinimalEventData>();
            event->module = 0;
            event->channel = static_cast<uint8_t>(i % 16);
            event->timeStampNs = static_cast<double>(i * 1000);
            event->energy = static_cast<uint16_t>(100 + i);
            event->energyShort = static_cast<uint16_t>(50 + i);
            events->push_back(std::move(event));
        }
        return events;
    }

    // Simulate receiving data and checking sequence
    SequenceGapDetector::Result ReceiveAndCheck(
        const std::unique_ptr<std::vector<uint8_t>>& data)
    {
        auto [events, seq] = receiver_->DecodeMinimal(data);
        if (!events) {
            return SequenceGapDetector::Result::BackwardsSequence;  // Error
        }
        return gap_detector_->Check(seq);
    }
};

// Test: Normal flow with no gaps
TEST_F(SequenceGapIntegrationTest, NormalFlowNoGap)
{
    // Send 10 messages in sequence
    for (int i = 0; i < 10; ++i) {
        auto events = CreateTestEvents(5);
        auto data = sender_->ProcessWithAutoSequence(events);
        ASSERT_NE(data, nullptr);

        auto result = ReceiveAndCheck(data);
        EXPECT_EQ(result, SequenceGapDetector::Result::Ok)
            << "Gap detected at message " << i;
    }

    EXPECT_EQ(gap_detector_->GetGapCount(), 0);
}

// Test: Simulated drop detects gap
TEST_F(SequenceGapIntegrationTest, SimulatedDropDetectsGap)
{
    // Send first message
    auto events1 = CreateTestEvents(5);
    auto data1 = sender_->ProcessWithAutoSequence(events1);
    EXPECT_EQ(ReceiveAndCheck(data1), SequenceGapDetector::Result::Ok);

    // Send second message (but don't receive it - simulates drop)
    auto events2 = CreateTestEvents(5);
    auto data2 = sender_->ProcessWithAutoSequence(events2);
    // Don't call ReceiveAndCheck(data2) - message is "dropped"

    // Send third message
    auto events3 = CreateTestEvents(5);
    auto data3 = sender_->ProcessWithAutoSequence(events3);

    // Receiver should detect gap
    auto result = ReceiveAndCheck(data3);
    EXPECT_EQ(result, SequenceGapDetector::Result::Gap);

    auto gap = gap_detector_->GetLastGap();
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->expected, 1);  // Expected sequence 1
    EXPECT_EQ(gap->received, 2);  // But received sequence 2
    EXPECT_EQ(gap->dropped_count, 1);
}

// Test: Multiple drops accumulate
TEST_F(SequenceGapIntegrationTest, MultipleDropsAccumulate)
{
    // Send and receive first message
    auto events1 = CreateTestEvents(5);
    auto data1 = sender_->ProcessWithAutoSequence(events1);
    EXPECT_EQ(ReceiveAndCheck(data1), SequenceGapDetector::Result::Ok);

    // Send 5 messages but drop them all
    for (int i = 0; i < 5; ++i) {
        auto events = CreateTestEvents(5);
        sender_->ProcessWithAutoSequence(events);  // Send but don't receive
    }

    // Send and receive message 7 (sequence = 6)
    auto events7 = CreateTestEvents(5);
    auto data7 = sender_->ProcessWithAutoSequence(events7);
    auto result = ReceiveAndCheck(data7);

    EXPECT_EQ(result, SequenceGapDetector::Result::Gap);
    EXPECT_EQ(gap_detector_->GetGapCount(), 1);

    auto gap = gap_detector_->GetLastGap();
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->dropped_count, 5);  // 5 messages were dropped
}

// Test: Reset allows new run
TEST_F(SequenceGapIntegrationTest, ResetAllowsNewRun)
{
    // First run: send some messages
    for (int i = 0; i < 3; ++i) {
        auto events = CreateTestEvents(5);
        auto data = sender_->ProcessWithAutoSequence(events);
        ReceiveAndCheck(data);
    }

    // Reset for new run
    sender_->ResetSequence();
    gap_detector_->Reset();

    // New run should start from 0 without gap
    auto events = CreateTestEvents(5);
    auto data = sender_->ProcessWithAutoSequence(events);
    auto result = ReceiveAndCheck(data);

    EXPECT_EQ(result, SequenceGapDetector::Result::Ok);
    EXPECT_EQ(gap_detector_->GetGapCount(), 0);
}

// Test: Large sequence numbers work correctly
TEST_F(SequenceGapIntegrationTest, LargeSequenceNumbers)
{
    // Use a large starting sequence
    for (uint64_t i = 0; i < 1000; ++i) {
        sender_->GetNextSequence();  // Advance counter
    }

    // Now send messages starting from sequence 1000
    for (int i = 0; i < 10; ++i) {
        auto events = CreateTestEvents(5);
        auto data = sender_->ProcessWithAutoSequence(events);
        auto result = ReceiveAndCheck(data);
        EXPECT_EQ(result, SequenceGapDetector::Result::Ok);
    }

    EXPECT_EQ(gap_detector_->GetGapCount(), 0);
}

// Test: Sequence gap with manual sequence numbers
TEST_F(SequenceGapIntegrationTest, ManualSequenceNumbers)
{
    // Send with explicit sequence numbers
    auto events1 = CreateTestEvents(5);
    auto data1 = sender_->Process(events1, 100);  // seq = 100
    EXPECT_EQ(ReceiveAndCheck(data1), SequenceGapDetector::Result::Ok);

    auto events2 = CreateTestEvents(5);
    auto data2 = sender_->Process(events2, 101);  // seq = 101
    EXPECT_EQ(ReceiveAndCheck(data2), SequenceGapDetector::Result::Ok);

    // Skip to sequence 105
    auto events3 = CreateTestEvents(5);
    auto data3 = sender_->Process(events3, 105);  // seq = 105, gap of 3
    auto result = ReceiveAndCheck(data3);

    EXPECT_EQ(result, SequenceGapDetector::Result::Gap);

    auto gap = gap_detector_->GetLastGap();
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->expected, 102);
    EXPECT_EQ(gap->received, 105);
    EXPECT_EQ(gap->dropped_count, 3);
}
