#include <gtest/gtest.h>
#include "../../lib/net/include/Serializer.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::MinimalEventData;

class FormatCompatibilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup serializers for different formats
        eventdata_serializer = std::make_unique<Serializer>();  // defaults to FORMAT_VERSION_EVENTDATA
        minimal_serializer = std::make_unique<Serializer>();    // we'll use DecodeMinimalEventData method
    }
    
    std::unique_ptr<Serializer> eventdata_serializer;
    std::unique_ptr<Serializer> minimal_serializer;
    
    // Helper function to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEvents(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        for (size_t i = 0; i < count; ++i) {
            events->push_back(std::make_unique<MinimalEventData>(
                static_cast<uint8_t>(i % 256),      // module
                static_cast<uint8_t>(i % 64),       // channel  
                static_cast<double>(i * 1000.0),    // timeStampNs
                static_cast<uint16_t>(i * 10),      // energy
                static_cast<uint16_t>(i * 5),       // energyShort
                static_cast<uint64_t>(i % 4)        // flags
            ));
        }
        return events;
    }
};

// TDD RED phase - Test that MinimalEventData creates format_version = 2
TEST_F(FormatCompatibilityTest, MinimalEventDataProducesCorrectFormatVersion) {
    // Create and encode MinimalEventData
    auto events = CreateTestMinimalEvents(1);
    auto encoded = minimal_serializer->Encode(events, 42);
    
    ASSERT_NE(encoded, nullptr);
    ASSERT_GE(encoded->size(), sizeof(BinaryDataHeader));
    
    // Extract header and verify format version
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(encoded->data());
    
    EXPECT_EQ(header->magic_number, BINARY_DATA_MAGIC_NUMBER);
    EXPECT_EQ(header->format_version, FORMAT_VERSION_MINIMAL_EVENTDATA);
    EXPECT_EQ(header->event_count, 1);
    EXPECT_EQ(header->sequence_number, 42);
}

// Test that wrong format version is properly rejected
TEST_F(FormatCompatibilityTest, RejectsWrongFormatVersion) {
    // Create MinimalEventData and encode it (format_version = 2)
    auto minimal_events = CreateTestMinimalEvents(1);
    auto encoded_minimal = minimal_serializer->Encode(minimal_events, 42);
    
    ASSERT_NE(encoded_minimal, nullptr);
    
    // Try to decode MinimalEventData with regular Decode method (expects format_version = 1)
    auto [decoded_eventdata, seq] = eventdata_serializer->Decode(encoded_minimal);
    
    // Should fail gracefully (return nullptr) due to format version mismatch
    EXPECT_EQ(decoded_eventdata, nullptr);
    EXPECT_EQ(seq, 0);
}

// Test that MinimalEventData decoding validates format version
TEST_F(FormatCompatibilityTest, MinimalDecoderValidatesFormatVersion) {
    // Create a fake header with wrong format version
    auto fake_data = std::make_unique<std::vector<uint8_t>>();
    fake_data->resize(sizeof(BinaryDataHeader) + 22); // Header + 1 MinimalEventData
    
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(fake_data->data());
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->format_version = FORMAT_VERSION_EVENTDATA; // Wrong version (should be 2)
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 1;
    header->compressed_size = 22;
    header->uncompressed_size = 22;
    header->checksum = 0;
    
    // Try to decode as MinimalEventData (should fail due to wrong format version)
    auto [decoded_events, seq] = minimal_serializer->DecodeMinimalEventData(fake_data);
    
    EXPECT_EQ(decoded_events, nullptr);
    EXPECT_EQ(seq, 0);
}

// Test round-trip compatibility within same format
TEST_F(FormatCompatibilityTest, MinimalEventDataRoundTripCompatibility) {
    // Create test data
    auto original_events = CreateTestMinimalEvents(3);
    
    // Store original values for comparison
    std::vector<MinimalEventData> expected_values;
    for (const auto& event : *original_events) {
        expected_values.push_back(*event);
    }
    
    // Encode
    auto encoded = minimal_serializer->Encode(original_events, 123);
    ASSERT_NE(encoded, nullptr);
    
    // Decode
    auto [decoded_events, seq] = minimal_serializer->DecodeMinimalEventData(encoded);
    ASSERT_NE(decoded_events, nullptr);
    ASSERT_EQ(decoded_events->size(), 3);
    EXPECT_EQ(seq, 123);
    
    // Verify data integrity
    for (size_t i = 0; i < 3; ++i) {
        const auto& decoded = *(*decoded_events)[i];
        const auto& expected = expected_values[i];
        
        EXPECT_EQ(decoded.module, expected.module);
        EXPECT_EQ(decoded.channel, expected.channel);
        EXPECT_EQ(decoded.timeStampNs, expected.timeStampNs);
        EXPECT_EQ(decoded.energy, expected.energy);
        EXPECT_EQ(decoded.energyShort, expected.energyShort);
        EXPECT_EQ(decoded.flags, expected.flags);
    }
}

// Test that corrupted format version is handled gracefully
TEST_F(FormatCompatibilityTest, HandlesCorruptedFormatVersion) {
    // Create valid MinimalEventData
    auto events = CreateTestMinimalEvents(1);
    auto encoded = minimal_serializer->Encode(events, 42);
    
    ASSERT_NE(encoded, nullptr);
    ASSERT_GE(encoded->size(), sizeof(BinaryDataHeader));
    
    // Corrupt the format version in the header
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(encoded->data());
    header->format_version = 999; // Invalid format version
    
    // Try to decode - should fail gracefully
    auto [decoded_events, seq] = minimal_serializer->DecodeMinimalEventData(encoded);
    
    EXPECT_EQ(decoded_events, nullptr);
    EXPECT_EQ(seq, 0);
}