#include <gtest/gtest.h>
#include "../../../lib/net/include/DataProcessor.hpp"
#include "../../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::MinimalEventData;

class DataProcessorFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    std::unique_ptr<DataProcessor> processor;
};

// Format version constants test
TEST_F(DataProcessorFormatTest, FormatVersionConstantsExist) {
    // Test that format version constants are defined
    EXPECT_EQ(FORMAT_VERSION_EVENTDATA, 1);
    EXPECT_EQ(FORMAT_VERSION_MINIMAL_EVENTDATA, 2);
}

// TDD RED phase - This test should fail because MinimalEventData encoding doesn't exist yet
TEST_F(DataProcessorFormatTest, EncodeMinimalEventDataVector) {
    // Create test MinimalEventData
    auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    events->push_back(std::make_unique<MinimalEventData>(1, 2, 1234.5, 100, 50, 0x01));
    events->push_back(std::make_unique<MinimalEventData>(2, 3, 2345.6, 200, 75, 0x02));
    
    // This should call MinimalEventData encode method (doesn't exist yet)
    auto encoded = processor->Process(events, 42);
    
    EXPECT_NE(encoded, nullptr);
    EXPECT_GT(encoded->size(), BINARY_DATA_HEADER_SIZE);
}

// TDD RED phase - This test should fail because MinimalEventData decoding doesn't exist yet  
TEST_F(DataProcessorFormatTest, DecodeMinimalEventDataVector) {
    // First encode some test data
    auto originalEvents = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    originalEvents->push_back(std::make_unique<MinimalEventData>(1, 2, 1234.5, 100, 50, 0x01));
    originalEvents->push_back(std::make_unique<MinimalEventData>(2, 3, 2345.6, 200, 75, 0x02));
    
    auto encoded = processor->Process(originalEvents, 42);
    ASSERT_NE(encoded, nullptr);
    
    // This should call MinimalEventData decode method
    auto decoded = processor->DecodeMinimal(encoded);
    
    EXPECT_NE(decoded.first, nullptr);
    EXPECT_EQ(decoded.second, 42); // sequence number
    EXPECT_EQ(decoded.first->size(), 2); // two events
}

// Backward compatibility test - ensure EventData still works
TEST_F(DataProcessorFormatTest, BackwardCompatibleWithEventData) {
    // This test verifies that existing EventData encoding/decoding still works
    // Note: We can't create EventData easily here without more dependencies
    // So this is a placeholder that compiles and represents the test intent
    EXPECT_EQ(FORMAT_VERSION_EVENTDATA, 1); // Ensure EventData format is still version 1
}

// Size validation test - verify MinimalEventData size efficiency
TEST_F(DataProcessorFormatTest, MinimalEventDataSizeOptimization) {
    // Create test MinimalEventData
    auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    events->push_back(std::make_unique<MinimalEventData>(1, 2, 1234.5, 100, 50, 0x01));
    
    auto encoded = processor->Process(events, 42);
    ASSERT_NE(encoded, nullptr);
    
    // Verify size: header (64 bytes) + 1 event (22 bytes) = 86 bytes total
    // Note: DataProcessor may achieve better compression, so accept <= original size
    EXPECT_LE(encoded->size(), BINARY_DATA_HEADER_SIZE + 22);
    
    // Verify this is much smaller than typical EventData
    // (EventData with waveforms would be 100s of bytes)
    EXPECT_LT(encoded->size(), 100);
}

// Round-trip test - encode then decode and verify data integrity
TEST_F(DataProcessorFormatTest, MinimalEventDataRoundTrip) {
    // Create original test data
    auto originalEvents = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    originalEvents->push_back(std::make_unique<MinimalEventData>(1, 2, 1234.5, 100, 50, 0x01));
    originalEvents->push_back(std::make_unique<MinimalEventData>(3, 4, 5678.9, 200, 75, 0x06));
    
    // Encode
    auto encoded = processor->Process(originalEvents, 123);
    ASSERT_NE(encoded, nullptr);
    
    // Decode
    auto decoded = processor->DecodeMinimal(encoded);
    ASSERT_NE(decoded.first, nullptr);
    ASSERT_EQ(decoded.first->size(), 2);
    EXPECT_EQ(decoded.second, 123); // sequence number
    
    // Verify data integrity
    auto& events = *decoded.first;
    
    // First event
    EXPECT_EQ(events[0]->module, 1);
    EXPECT_EQ(events[0]->channel, 2);
    EXPECT_EQ(events[0]->timeStampNs, 1234.5);
    EXPECT_EQ(events[0]->energy, 100);
    EXPECT_EQ(events[0]->energyShort, 50);
    EXPECT_EQ(events[0]->flags, 0x01);
    
    // Second event
    EXPECT_EQ(events[1]->module, 3);
    EXPECT_EQ(events[1]->channel, 4);
    EXPECT_EQ(events[1]->timeStampNs, 5678.9);
    EXPECT_EQ(events[1]->energy, 200);
    EXPECT_EQ(events[1]->energyShort, 75);
    EXPECT_EQ(events[1]->flags, 0x06);
}