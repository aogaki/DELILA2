/**
 * @file test_data_processor_sequence.cpp
 * @brief Unit tests for DataProcessor sequence number management
 *
 * TDD Phase: RED - Write tests first
 */

#include <gtest/gtest.h>

#include "DataProcessor.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

class DataProcessorSequenceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        processor_ = std::make_unique<DataProcessor>();
    }

    std::unique_ptr<DataProcessor> processor_;

    // Helper to create test events
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestEvents(size_t count)
    {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
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
};

// Test: GetNextSequence starts at 0
TEST_F(DataProcessorSequenceTest, GetNextSequenceStartsAtZero)
{
    EXPECT_EQ(processor_->GetNextSequence(), 0);
}

// Test: GetNextSequence increments
TEST_F(DataProcessorSequenceTest, GetNextSequenceIncrements)
{
    EXPECT_EQ(processor_->GetNextSequence(), 0);
    EXPECT_EQ(processor_->GetNextSequence(), 1);
    EXPECT_EQ(processor_->GetNextSequence(), 2);
    EXPECT_EQ(processor_->GetNextSequence(), 3);
}

// Test: ResetSequence resets to zero
TEST_F(DataProcessorSequenceTest, ResetSequenceResetsToZero)
{
    processor_->GetNextSequence();
    processor_->GetNextSequence();
    processor_->GetNextSequence();

    processor_->ResetSequence();

    EXPECT_EQ(processor_->GetNextSequence(), 0);
}

// Test: GetCurrentSequence returns current without incrementing
TEST_F(DataProcessorSequenceTest, GetCurrentSequenceReturnsCurrentValue)
{
    processor_->GetNextSequence();  // 0
    processor_->GetNextSequence();  // 1

    // GetCurrentSequence should return next value without incrementing
    EXPECT_EQ(processor_->GetCurrentSequence(), 2);
    EXPECT_EQ(processor_->GetCurrentSequence(), 2);  // Same value
    EXPECT_EQ(processor_->GetCurrentSequence(), 2);  // Still same
}

// Test: ProcessWithAutoSequence uses internal counter
TEST_F(DataProcessorSequenceTest, ProcessWithAutoSequenceUsesInternalCounter)
{
    auto events1 = CreateTestEvents(5);
    auto events2 = CreateTestEvents(5);
    auto events3 = CreateTestEvents(5);

    auto data1 = processor_->ProcessWithAutoSequence(events1);
    auto data2 = processor_->ProcessWithAutoSequence(events2);
    auto data3 = processor_->ProcessWithAutoSequence(events3);

    ASSERT_NE(data1, nullptr);
    ASSERT_NE(data2, nullptr);
    ASSERT_NE(data3, nullptr);

    // Decode and verify sequence numbers
    auto [decoded1, seq1] = processor_->DecodeMinimal(data1);
    auto [decoded2, seq2] = processor_->DecodeMinimal(data2);
    auto [decoded3, seq3] = processor_->DecodeMinimal(data3);

    EXPECT_EQ(seq1, 0);
    EXPECT_EQ(seq2, 1);
    EXPECT_EQ(seq3, 2);
}

// Test: Header contains correct sequence number
TEST_F(DataProcessorSequenceTest, HeaderContainsCorrectSequence)
{
    auto events = CreateTestEvents(3);
    uint64_t expected_seq = 42;

    auto data = processor_->Process(events, expected_seq);
    ASSERT_NE(data, nullptr);

    // Decode and verify
    auto [decoded, actual_seq] = processor_->DecodeMinimal(data);
    EXPECT_EQ(actual_seq, expected_seq);
}
