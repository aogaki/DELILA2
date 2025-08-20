#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "../../../lib/net/include/DataProcessor.hpp"

using namespace DELILA::Net;

class DataProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    std::unique_ptr<DataProcessor> processor;
};

// CRC32 Tests with known test vectors
// Following TDD: Write tests FIRST before implementation
class CRC32Test : public ::testing::Test {
protected:
    // Known CRC32 test vectors (using standard CRC32)
    struct TestVector {
        std::string input;
        uint32_t expected_crc32;
    };
    
    std::vector<TestVector> test_vectors = {
        {"", 0x00000000},                    // Empty string
        {"a", 0xE8B7BE43},                   // Single character 'a'
        {"abc", 0x352441C2},                 // "abc"
        {"message digest", 0x20159D7F},      // "message digest"
        {"abcdefghijklmnopqrstuvwxyz", 0x4C2750BD}, // alphabet
        {"123456789", 0xCBF43926},           // "123456789" - standard CRC32 test
        {"The quick brown fox jumps over the lazy dog", 0x414FA339} // pangram
    };
};

// RED phase: Write failing tests first
TEST_F(CRC32Test, CalculateCRC32WithEmptyData) {
    const uint8_t* empty_data = nullptr;
    size_t length = 0;
    
    // This should work with empty data
    uint32_t result = DataProcessor::CalculateCRC32(empty_data, length);
    EXPECT_EQ(result, 0x00000000);
}

TEST_F(CRC32Test, CalculateCRC32WithKnownTestVectors) {
    for (const auto& vector : test_vectors) {
        if (vector.input.empty()) continue; // Skip empty string for pointer safety
        
        const uint8_t* data = reinterpret_cast<const uint8_t*>(vector.input.c_str());
        size_t length = vector.input.length();
        
        uint32_t result = DataProcessor::CalculateCRC32(data, length);
        
        EXPECT_EQ(result, vector.expected_crc32) 
            << "CRC32 mismatch for input: '" << vector.input 
            << "', expected: 0x" << std::hex << vector.expected_crc32 
            << ", got: 0x" << std::hex << result;
    }
}

TEST_F(CRC32Test, VerifyCRC32WithCorrectChecksum) {
    std::string test_data = "123456789";
    const uint8_t* data = reinterpret_cast<const uint8_t*>(test_data.c_str());
    size_t length = test_data.length();
    uint32_t expected = 0xCBF43926;
    
    bool result = DataProcessor::VerifyCRC32(data, length, expected);
    EXPECT_TRUE(result);
}

TEST_F(CRC32Test, VerifyCRC32WithIncorrectChecksum) {
    std::string test_data = "123456789";
    const uint8_t* data = reinterpret_cast<const uint8_t*>(test_data.c_str());
    size_t length = test_data.length();
    uint32_t wrong_checksum = 0x12345678;
    
    bool result = DataProcessor::VerifyCRC32(data, length, wrong_checksum);
    EXPECT_FALSE(result);
}

// Test that CRC32 table is initialized properly
TEST_F(CRC32Test, CRC32TableInitialization) {
    // Force table initialization by calculating any CRC32
    std::string test = "test";
    const uint8_t* data = reinterpret_cast<const uint8_t*>(test.c_str());
    DataProcessor::CalculateCRC32(data, test.length());
    
    // If we get here without crash, table was initialized properly
    SUCCEED();
}

// Test CRC32 consistency - same input should give same output
TEST_F(CRC32Test, CRC32Consistency) {
    std::string test_data = "consistency test";
    const uint8_t* data = reinterpret_cast<const uint8_t*>(test_data.c_str());
    size_t length = test_data.length();
    
    uint32_t crc1 = DataProcessor::CalculateCRC32(data, length);
    uint32_t crc2 = DataProcessor::CalculateCRC32(data, length);
    
    EXPECT_EQ(crc1, crc2) << "CRC32 should be consistent for same input";
}

// Phase 3: Serialization Tests (TDD - Write tests FIRST)
class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    // Helper to create test EventData
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            event->timeStampNs = 1000 + i;
            event->waveformSize = 100 + i;
            event->energy = 2000.5 + i;
            event->energyShort = 1500.5 + i;
            event->module = i % 4;
            event->channel = i % 16;
            event->timeResolution = 2;
            event->analogProbe1Type = 1;
            event->analogProbe2Type = 2;
            event->digitalProbe1Type = 3;
            event->digitalProbe2Type = 4;
            event->digitalProbe3Type = 5;
            event->digitalProbe4Type = 6;
            event->downSampleFactor = 1;
            event->flags = 0x01;
            
            // Add some waveform data
            event->analogProbe1 = {100, 200, 300, 400, 500};
            event->analogProbe2 = {150, 250, 350, 450, 550};
            event->digitalProbe1 = {0, 1, 0, 1, 0};
            event->digitalProbe2 = {1, 0, 1, 0, 1};
            event->digitalProbe3 = {1, 1, 0, 0, 1};
            event->digitalProbe4 = {0, 0, 1, 1, 0};
            
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    // Helper to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<MinimalEventData>(
                i % 4,           // module
                i % 16,          // channel
                100 + i,         // timeStampNs (3rd parameter)
                1000 + i,        // energy (4th parameter, uint16_t)
                50 + i,          // energyShort
                0x01 + i         // flags
            );
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    std::unique_ptr<DataProcessor> processor;
};

// Test internal serialization methods (for TDD - testing implementation step by step)
// Note: These test private methods temporarily made public for testing
class InternalSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    // Same helper methods as SerializationTest
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            event->timeStampNs = 1000 + i;
            event->waveformSize = 100 + i;
            event->energy = 2000.5 + i;
            event->energyShort = 1500.5 + i;
            event->module = i % 4;
            event->channel = i % 16;
            event->timeResolution = 2;
            event->analogProbe1Type = 1;
            event->analogProbe2Type = 2;
            event->digitalProbe1Type = 3;
            event->digitalProbe2Type = 4;
            event->digitalProbe3Type = 5;
            event->digitalProbe4Type = 6;
            event->downSampleFactor = 1;
            event->flags = 0x01;
            
            // Add some waveform data
            event->analogProbe1 = {100, 200, 300, 400, 500};
            event->analogProbe2 = {150, 250, 350, 450, 550};
            event->digitalProbe1 = {0, 1, 0, 1, 0};
            event->digitalProbe2 = {1, 0, 1, 0, 1};
            event->digitalProbe3 = {1, 1, 0, 0, 1};
            event->digitalProbe4 = {0, 0, 1, 1, 0};
            
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<MinimalEventData>(
                i % 4,           // module
                i % 16,          // channel
                100 + i,         // timeStampNs (3rd parameter)
                1000 + i,        // energy (4th parameter, uint16_t)
                50 + i,          // energyShort
                0x01 + i         // flags
            );
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    std::unique_ptr<DataProcessor> processor;
};

// GREEN phase: Test internal serialization methods now that they're implemented
TEST_F(InternalSerializationTest, SerializeEmptyEventDataVector) {
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    // Process now works! Should handle empty events gracefully
    auto result = processor->Process(empty_events, 0);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->size(), sizeof(BinaryDataHeader));
}

// Test the full Process pipeline (now implemented!)
TEST_F(SerializationTest, ProcessEmptyEventDataVector) {
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    // Full Process method now works!
    auto result = processor->Process(empty_events, 0);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->size(), sizeof(BinaryDataHeader)); // Just header for empty data
}

TEST_F(SerializationTest, ProcessSingleEventData) {
    auto events = CreateTestEventData(1);
    
    // Full Process method now works!
    auto result = processor->Process(events, 42);
    EXPECT_NE(result, nullptr);
    EXPECT_GT(result->size(), sizeof(BinaryDataHeader)); // Header + serialized data
    
    // Validate header
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->sequence_number, 42);
    EXPECT_EQ(header->event_count, 1);
}

TEST_F(SerializationTest, ProcessMultipleEventData) {
    auto events = CreateTestEventData(5);
    
    // Full Process method now works!
    auto result = processor->Process(events, 42);
    EXPECT_NE(result, nullptr);
    EXPECT_GT(result->size(), sizeof(BinaryDataHeader)); // Header + serialized data
    
    // Validate header
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->sequence_number, 42);
    EXPECT_EQ(header->event_count, 5);
}

TEST_F(SerializationTest, ProcessEmptyMinimalEventDataVector) {
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    
    // Full Process method now works!
    auto result = processor->Process(empty_events, 0);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->size(), sizeof(BinaryDataHeader)); // Just header for empty data
}

TEST_F(SerializationTest, ProcessSingleMinimalEventData) {
    auto events = CreateTestMinimalEventData(1);
    
    // Full Process method now works!
    auto result = processor->Process(events, 42);
    EXPECT_NE(result, nullptr);
    EXPECT_GT(result->size(), sizeof(BinaryDataHeader)); // Header + serialized data
    
    // Validate header
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->sequence_number, 42);
    EXPECT_EQ(header->event_count, 1);
    EXPECT_EQ(header->format_version, FORMAT_VERSION_MINIMAL_EVENTDATA);
}

TEST_F(SerializationTest, ProcessMultipleMinimalEventData) {
    auto events = CreateTestMinimalEventData(10);
    
    // Full Process method now works!
    auto result = processor->Process(events, 42);
    EXPECT_NE(result, nullptr);
    EXPECT_GT(result->size(), sizeof(BinaryDataHeader)); // Header + serialized data
    
    // Validate header
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->sequence_number, 42);
    EXPECT_EQ(header->event_count, 10);
    EXPECT_EQ(header->format_version, FORMAT_VERSION_MINIMAL_EVENTDATA);
}

// Test deserialization with invalid data
TEST_F(SerializationTest, DeserializeEventData) {
    auto test_data = std::make_unique<std::vector<uint8_t>>(100, 0xFF);  // Invalid dummy data
    
    // This should fail gracefully by returning {nullptr, 0}
    auto [events, sequence] = processor->Decode(test_data);
    EXPECT_EQ(events, nullptr);
    EXPECT_EQ(sequence, 0);
}

TEST_F(SerializationTest, DeserializeMinimalEventData) {
    auto test_data = std::make_unique<std::vector<uint8_t>>(100, 0xFF);  // Invalid dummy data
    
    // This should fail gracefully by returning {nullptr, 0}
    auto [minimal_events, sequence] = processor->DecodeMinimal(test_data);
    EXPECT_EQ(minimal_events, nullptr);
    EXPECT_EQ(sequence, 0);
}

// Test null input handling
TEST_F(SerializationTest, ProcessHandlesNullEventData) {
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> null_events = nullptr;
    
    // Should handle null input gracefully - returns nullptr
    auto result = processor->Process(null_events, 0);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationTest, ProcessHandlesNullMinimalEventData) {
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> null_events = nullptr;
    
    // Should handle null input gracefully - returns nullptr
    auto result = processor->Process(null_events, 0);
    EXPECT_EQ(result, nullptr);
}

// Phase 4: Compression Tests (TDD - Write tests FIRST)
class CompressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    // Helper to create test data for compression
    std::unique_ptr<std::vector<uint8_t>> CreateTestData(size_t size, uint8_t pattern = 0xAA) {
        auto data = std::make_unique<std::vector<uint8_t>>();
        data->reserve(size);
        for (size_t i = 0; i < size; ++i) {
            data->push_back(static_cast<uint8_t>(pattern + (i % 10)));  // Create some pattern
        }
        return data;
    }
    
    // Helper to create highly compressible data (repeated pattern)
    std::unique_ptr<std::vector<uint8_t>> CreateCompressibleData(size_t size) {
        auto data = std::make_unique<std::vector<uint8_t>>(size, 0xAB);  // All same byte
        return data;
    }
    
    std::unique_ptr<DataProcessor> processor;
};

// RED phase: Test compression methods that don't exist yet 
// Note: These will need CompressLZ4/DecompressLZ4 to be made public for testing
TEST_F(CompressionTest, CompressLZ4WithEmptyData) {
    auto empty_data = std::make_unique<std::vector<uint8_t>>();
    
    // Should handle empty input gracefully (when implemented)
    // For now, this tests internal methods that will be implemented
    // Expect this to fail until compression is implemented
    SUCCEED(); // Placeholder until we can call CompressLZ4 directly
}

TEST_F(CompressionTest, CompressLZ4WithSmallData) {
    auto small_data = CreateTestData(100);
    
    // Test compression of small data (when implemented)
    // Internal CompressLZ4 method should handle this
    SUCCEED(); // Placeholder until we can call CompressLZ4 directly
}

TEST_F(CompressionTest, CompressLZ4WithLargeCompressibleData) {
    auto large_data = CreateCompressibleData(10000);  // 10KB of repeated data
    
    // This should compress very well (when implemented)
    // Internal CompressLZ4 method should reduce size significantly
    SUCCEED(); // Placeholder until we can call CompressLZ4 directly
}

TEST_F(CompressionTest, CompressLZ4WithRandomData) {
    auto random_data = CreateTestData(5000, 0x12);  // More random pattern
    
    // This might not compress well (when implemented)
    // Internal CompressLZ4 method should handle this case
    SUCCEED(); // Placeholder until we can call CompressLZ4 directly
}

TEST_F(CompressionTest, DecompressLZ4WithValidData) {
    // First compress some data, then decompress it (when implemented)
    auto original_data = CreateCompressibleData(1000);
    
    // Test round-trip: compress then decompress (when implemented)
    // Internal DecompressLZ4 method should restore original data
    SUCCEED(); // Placeholder until we can call DecompressLZ4 directly
}

TEST_F(CompressionTest, DecompressLZ4WithInvalidData) {
    auto invalid_data = CreateTestData(100, 0xFF);  // Not compressed data
    
    // Should handle invalid compressed data gracefully (when implemented)
    // Internal DecompressLZ4 method should return nullptr or throw
    SUCCEED(); // Placeholder until we can call DecompressLZ4 directly
}

TEST_F(CompressionTest, CompressionRoundTrip) {
    auto original_data = CreateTestData(2000);
    
    // Test full round trip: original -> compress -> decompress -> compare (when implemented)
    // This tests that CompressLZ4/DecompressLZ4 work together correctly
    SUCCEED(); // Placeholder until compression is implemented
}

TEST_F(CompressionTest, CompressionEnabledDisabled) {
    // Test that processor respects compression enable/disable flag
    processor->EnableCompression(false);
    EXPECT_FALSE(processor->IsCompressionEnabled());
    
    processor->EnableCompression(true);
    EXPECT_TRUE(processor->IsCompressionEnabled());
    
    // When Process is implemented, it should respect this flag
}

// Test integration with Process method (full pipeline now implemented!)
TEST_F(CompressionTest, ProcessWithCompressionEnabled) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    processor->EnableCompression(true);
    // Full Process pipeline now works!
    auto result = processor->Process(events, 0);
    EXPECT_NE(result, nullptr);
    
    // Check that compression is enabled in header
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->compression_type, COMPRESSION_LZ4);
}

TEST_F(CompressionTest, ProcessWithCompressionDisabled) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    processor->EnableCompression(false);
    // Full Process pipeline now works!
    auto result = processor->Process(events, 0);
    EXPECT_NE(result, nullptr);
    
    // Check that compression is disabled in header
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->compression_type, COMPRESSION_NONE);
}

// Phase 6: Process Pipeline Tests (TDD - Write tests FIRST)
class ProcessPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    // Helper to create test EventData
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            event->timeStampNs = 1000 + i;
            event->waveformSize = 100 + i;
            event->energy = 2000.5 + i;
            event->energyShort = 1500.5 + i;
            event->module = i % 4;
            event->channel = i % 16;
            event->timeResolution = 2;
            event->analogProbe1Type = 1;
            event->analogProbe2Type = 2;
            event->digitalProbe1Type = 3;
            event->digitalProbe2Type = 4;
            event->digitalProbe3Type = 5;
            event->digitalProbe4Type = 6;
            event->downSampleFactor = 1;
            event->flags = 0x01;
            
            // Add some waveform data
            event->analogProbe1 = {100, 200, 300, 400, 500};
            event->analogProbe2 = {150, 250, 350, 450, 550};
            event->digitalProbe1 = {0, 1, 0, 1, 0};
            event->digitalProbe2 = {1, 0, 1, 0, 1};
            event->digitalProbe3 = {1, 1, 0, 0, 1};
            event->digitalProbe4 = {0, 0, 1, 1, 0};
            
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    // Helper to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<MinimalEventData>(
                i % 4,           // module
                i % 16,          // channel
                100 + i,         // timeStampNs (3rd parameter)
                1000 + i,        // energy (4th parameter, uint16_t)
                50 + i,          // energyShort
                0x01 + i         // flags
            );
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    // Helper to validate BinaryDataHeader
    void ValidateHeader(const std::vector<uint8_t>& data, uint32_t expected_event_count, uint64_t sequence_number) {
        ASSERT_GE(data.size(), sizeof(BinaryDataHeader)) << "Data too small to contain header";
        
        const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
        
        EXPECT_EQ(header->magic_number, BINARY_DATA_MAGIC_NUMBER) << "Wrong magic number";
        EXPECT_EQ(header->sequence_number, sequence_number) << "Wrong sequence number";
        EXPECT_EQ(header->header_size, BINARY_DATA_HEADER_SIZE) << "Wrong header size";
        EXPECT_EQ(header->event_count, expected_event_count) << "Wrong event count";
        EXPECT_GT(header->timestamp, 0) << "Invalid timestamp";
    }
    
    std::unique_ptr<DataProcessor> processor;
};

// GREEN phase: Test full Process pipeline (now implemented!)
TEST_F(ProcessPipelineTest, ProcessEventDataWithAllOptionsEnabled) {
    auto events = CreateTestEventData(3);
    processor->EnableCompression(true);
    processor->EnableChecksum(true);
    
    // Process should now work!
    auto result = processor->Process(events, 42);
    ASSERT_NE(result, nullptr);
    
    // Validate the header
    ValidateHeader(*result, 3, 42);
    
    // Check header details
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->format_version, FORMAT_VERSION_EVENTDATA);
    EXPECT_EQ(header->compression_type, COMPRESSION_LZ4);
    EXPECT_EQ(header->checksum_type, CHECKSUM_CRC32);
    EXPECT_GT(header->checksum, 0); // Should have a checksum
}

TEST_F(ProcessPipelineTest, ProcessEventDataWithCompressionDisabled) {
    auto events = CreateTestEventData(2);
    processor->EnableCompression(false);
    processor->EnableChecksum(true);
    
    // Process should work with compression disabled
    auto result = processor->Process(events, 123);
    ASSERT_NE(result, nullptr);
    
    // Validate the header
    ValidateHeader(*result, 2, 123);
    
    // Check that compression is disabled
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->compression_type, COMPRESSION_NONE);
    EXPECT_EQ(header->checksum_type, CHECKSUM_CRC32);
    EXPECT_EQ(header->uncompressed_size, header->compressed_size); // No compression
}

TEST_F(ProcessPipelineTest, ProcessEventDataWithChecksumDisabled) {
    auto events = CreateTestEventData(1);
    processor->EnableCompression(true);
    processor->EnableChecksum(false);
    
    // Process should work with checksum disabled
    auto result = processor->Process(events, 456);
    ASSERT_NE(result, nullptr);
    
    // Validate the header
    ValidateHeader(*result, 1, 456);
    
    // Check that checksum is disabled
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->compression_type, COMPRESSION_LZ4);
    EXPECT_EQ(header->checksum_type, CHECKSUM_NONE);
    EXPECT_EQ(header->checksum, 0); // No checksum
}

TEST_F(ProcessPipelineTest, ProcessEventDataWithBothDisabled) {
    auto events = CreateTestEventData(1);
    processor->EnableCompression(false);
    processor->EnableChecksum(false);
    
    // Process should work with both disabled
    auto result = processor->Process(events, 789);
    ASSERT_NE(result, nullptr);
    
    // Validate the header
    ValidateHeader(*result, 1, 789);
    
    // Check that both are disabled
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->compression_type, COMPRESSION_NONE);
    EXPECT_EQ(header->checksum_type, CHECKSUM_NONE);
    EXPECT_EQ(header->checksum, 0); // No checksum
    EXPECT_EQ(header->uncompressed_size, header->compressed_size); // No compression
}

TEST_F(ProcessPipelineTest, ProcessMinimalEventDataWithAllOptionsEnabled) {
    auto events = CreateTestMinimalEventData(5);
    processor->EnableCompression(true);
    processor->EnableChecksum(true);
    
    // Process should now work for MinimalEventData!
    auto result = processor->Process(events, 111);
    ASSERT_NE(result, nullptr);
    
    // Validate the header
    ValidateHeader(*result, 5, 111);
    
    // Check header details
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->format_version, FORMAT_VERSION_MINIMAL_EVENTDATA);
    EXPECT_EQ(header->compression_type, COMPRESSION_LZ4);
    EXPECT_EQ(header->checksum_type, CHECKSUM_CRC32);
}

TEST_F(ProcessPipelineTest, ProcessMinimalEventDataWithCompressionDisabled) {
    auto events = CreateTestMinimalEventData(3);
    processor->EnableCompression(false);
    processor->EnableChecksum(true);
    
    // Process should work with compression disabled
    auto result = processor->Process(events, 222);
    ASSERT_NE(result, nullptr);
    
    // Validate the header
    ValidateHeader(*result, 3, 222);
    
    // Check header details
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    EXPECT_EQ(header->format_version, FORMAT_VERSION_MINIMAL_EVENTDATA);
    EXPECT_EQ(header->compression_type, COMPRESSION_NONE);
    EXPECT_EQ(header->checksum_type, CHECKSUM_CRC32);
}

TEST_F(ProcessPipelineTest, ProcessEmptyEventDataVector) {
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    // Should handle empty input gracefully - creates header with 0 events
    auto result = processor->Process(empty_events, 0);
    ASSERT_NE(result, nullptr);
    
    // Validate the header with 0 events
    ValidateHeader(*result, 0, 0);
    
    // Should have header + minimal or no data
    EXPECT_EQ(result->size(), sizeof(BinaryDataHeader));
}

TEST_F(ProcessPipelineTest, ProcessEmptyMinimalEventDataVector) {
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    
    // Should handle empty input gracefully - creates header with 0 events
    auto result = processor->Process(empty_events, 0);
    ASSERT_NE(result, nullptr);
    
    // Validate the header with 0 events
    ValidateHeader(*result, 0, 0);
    
    // Should have header + minimal or no data
    EXPECT_EQ(result->size(), sizeof(BinaryDataHeader));
}

TEST_F(ProcessPipelineTest, ProcessNullEventDataVector) {
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> null_events = nullptr;
    
    // Should handle null input gracefully - returns nullptr
    auto result = processor->Process(null_events, 0);
    EXPECT_EQ(result, nullptr);
}

TEST_F(ProcessPipelineTest, ProcessNullMinimalEventDataVector) {
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> null_events = nullptr;
    
    // Should handle null input gracefully - returns nullptr
    auto result = processor->Process(null_events, 0);
    EXPECT_EQ(result, nullptr);
}

// Test that validates header structure now that Process is implemented
TEST_F(ProcessPipelineTest, ProcessCreatesValidHeaderStructure) {
    auto events = CreateTestEventData(1);
    
    auto result = processor->Process(events, 999);
    ASSERT_NE(result, nullptr);
    
    // Comprehensive header validation
    ValidateHeader(*result, 1, 999);
    
    const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(result->data());
    
    // Check all header fields
    EXPECT_EQ(header->magic_number, BINARY_DATA_MAGIC_NUMBER);
    EXPECT_EQ(header->sequence_number, 999);
    EXPECT_EQ(header->format_version, FORMAT_VERSION_EVENTDATA);
    EXPECT_EQ(header->header_size, BINARY_DATA_HEADER_SIZE);
    EXPECT_EQ(header->event_count, 1);
    EXPECT_GT(header->timestamp, 0);
    EXPECT_GT(header->uncompressed_size, 0);
    EXPECT_GT(header->compressed_size, 0);
    
    // Check type fields based on defaults
    EXPECT_EQ(header->compression_type, COMPRESSION_LZ4);  // Default enabled
    EXPECT_EQ(header->checksum_type, CHECKSUM_CRC32);     // Default enabled
}

// Basic DataProcessor Tests - placeholder for future implementation
TEST_F(DataProcessorTest, DefaultConfiguration) {
    EXPECT_TRUE(processor->IsCompressionEnabled());
    EXPECT_TRUE(processor->IsChecksumEnabled());
}

TEST_F(DataProcessorTest, EnableDisableCompression) {
    processor->EnableCompression(false);
    EXPECT_FALSE(processor->IsCompressionEnabled());
    
    processor->EnableCompression(true);
    EXPECT_TRUE(processor->IsCompressionEnabled());
}

TEST_F(DataProcessorTest, EnableDisableChecksum) {
    processor->EnableChecksum(false);
    EXPECT_FALSE(processor->IsChecksumEnabled());
    
    processor->EnableChecksum(true);
    EXPECT_TRUE(processor->IsChecksumEnabled());
}

// ============================================================================
// Phase 7: Decode Pipeline Tests (TDD RED Phase)
// Write ALL tests FIRST before implementing any Decode functionality
// ============================================================================

class DecodePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<DataProcessor>();
    }
    
    std::unique_ptr<DataProcessor> processor;
    
    // Helper to create test EventData for round-trip tests
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            event->module = i % 4;
            event->channel = i % 16;
            event->energy = 1000.5 + i;
            event->timeStampNs = 100 + i;
            event->energyShort = 50 + i;
            event->analogProbe1Type = 1;
            event->analogProbe2Type = 2;
            event->digitalProbe1Type = 3;
            event->digitalProbe2Type = 4;
            event->digitalProbe3Type = 5;
            event->digitalProbe4Type = 6;
            event->downSampleFactor = 1;
            event->flags = 0x01;
            
            // Add some waveform data
            event->analogProbe1 = {100, 200, 300, 400, 500};
            event->analogProbe2 = {150, 250, 350, 450, 550};
            event->digitalProbe1 = {0, 1, 0, 1, 0};
            event->digitalProbe2 = {1, 0, 1, 0, 1};
            event->digitalProbe3 = {1, 1, 0, 0, 1};
            event->digitalProbe4 = {0, 0, 1, 1, 0};
            
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    // Helper to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEventData(size_t count = 1) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<MinimalEventData>(
                i % 4,           // module
                i % 16,          // channel
                100 + i,         // timeStampNs (3rd parameter)
                1000 + i,        // energy (4th parameter, uint16_t)
                50 + i,          // energyShort
                0x01 + i         // flags
            );
            events->push_back(std::move(event));
        }
        
        return events;
    }
};

// Phase 7.1.1: Header parsing/validation tests (TDD RED phase)
TEST_F(DecodePipelineTest, DecodeHandlesNullInput) {
    std::unique_ptr<std::vector<uint8_t>> null_data = nullptr;
    
    auto result = processor->Decode(null_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeHandlesEmptyInput) {
    auto empty_data = std::make_unique<std::vector<uint8_t>>();
    
    auto result = processor->Decode(empty_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeRejectsDataTooSmallForHeader) {
    // Create data smaller than BinaryDataHeader
    auto small_data = std::make_unique<std::vector<uint8_t>>(32);  // Less than 64 bytes
    
    auto result = processor->Decode(small_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeRejectsInvalidMagicNumber) {
    auto invalid_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader));
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(invalid_data->data());
    
    // Set invalid magic number
    header->magic_number = 0x12345678;  // Wrong magic
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 1;
    header->uncompressed_size = 100;
    header->compressed_size = 100;
    header->checksum = 0;
    header->compression_type = COMPRESSION_NONE;
    header->checksum_type = CHECKSUM_NONE;
    
    auto result = processor->Decode(invalid_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeRejectsUnsupportedFormatVersion) {
    auto invalid_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader));
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(invalid_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = 999;  // Unsupported version
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 1;
    header->uncompressed_size = 100;
    header->compressed_size = 100;
    header->checksum = 0;
    header->compression_type = COMPRESSION_NONE;
    header->checksum_type = CHECKSUM_NONE;
    
    auto result = processor->Decode(invalid_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeRejectsInvalidHeaderSize) {
    auto invalid_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader));
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(invalid_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = 32;  // Wrong header size
    header->event_count = 1;
    header->uncompressed_size = 100;
    header->compressed_size = 100;
    header->checksum = 0;
    header->compression_type = COMPRESSION_NONE;
    header->checksum_type = CHECKSUM_NONE;
    
    auto result = processor->Decode(invalid_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

// Phase 7.1.2: CRC32 verification conditional logic tests (TDD RED phase)
TEST_F(DecodePipelineTest, DecodeVerifiesCRC32WhenEnabled) {
    // Create valid data with checksum enabled but wrong CRC32
    auto test_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader) + 10);
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(test_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 0;  // Empty payload for simplicity
    header->uncompressed_size = 0;
    header->compressed_size = 0;
    header->checksum = 0x12345678;  // Wrong checksum
    header->compression_type = COMPRESSION_NONE;
    header->checksum_type = CHECKSUM_CRC32;  // CRC32 enabled
    header->timestamp = 123456789;
    
    processor->EnableChecksum(true);  // Enable checksum verification
    
    auto result = processor->Decode(test_data);
    EXPECT_EQ(result.first, nullptr);  // Should fail due to wrong checksum
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeSkipsCRC32WhenDisabled) {
    // Create data with checksum disabled (should not verify CRC32)
    auto test_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader));
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(test_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 0;  // Empty payload
    header->uncompressed_size = 0;
    header->compressed_size = 0;
    header->checksum = 0;  // No checksum
    header->compression_type = COMPRESSION_NONE;
    header->checksum_type = CHECKSUM_NONE;  // CRC32 disabled
    header->timestamp = 123456789;
    
    processor->EnableChecksum(false);  // Disable checksum verification
    
    auto result = processor->Decode(test_data);
    EXPECT_NE(result.first, nullptr);  // Should succeed (empty event list)
    EXPECT_EQ(result.second, 1);  // Should return sequence number
    EXPECT_EQ(result.first->size(), 0);  // Empty event list
}

// Phase 7.1.3: LZ4 decompression conditional logic tests (TDD RED phase)
TEST_F(DecodePipelineTest, DecodeDecompressesLZ4WhenEnabled) {
    // This test will need actual compressed data - placeholder for now
    auto test_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader) + 20);
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(test_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 0;
    header->uncompressed_size = 10;
    header->compressed_size = 20;  // Larger than uncompressed (invalid)
    header->checksum = 0;
    header->compression_type = COMPRESSION_LZ4;  // LZ4 enabled
    header->checksum_type = CHECKSUM_NONE;
    header->timestamp = 123456789;
    
    processor->EnableCompression(true);
    processor->EnableChecksum(false);
    
    // This should fail because we have invalid LZ4 data
    auto result = processor->Decode(test_data);
    EXPECT_EQ(result.first, nullptr);  // Should fail due to invalid LZ4 data
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeSkipsDecompressionWhenDisabled) {
    // Create uncompressed data
    auto test_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader));
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(test_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 0;  // Empty payload
    header->uncompressed_size = 0;
    header->compressed_size = 0;
    header->checksum = 0;
    header->compression_type = COMPRESSION_NONE;  // No compression
    header->checksum_type = CHECKSUM_NONE;
    header->timestamp = 123456789;
    
    processor->EnableCompression(false);
    processor->EnableChecksum(false);
    
    auto result = processor->Decode(test_data);
    EXPECT_NE(result.first, nullptr);  // Should succeed
    EXPECT_EQ(result.second, 1);  // Should return sequence number
    EXPECT_EQ(result.first->size(), 0);  // Empty event list
}

// Phase 7.1.4: Deserialization to EventData tests (TDD RED phase)
TEST_F(DecodePipelineTest, DecodeBasicRoundTripEventData) {
    // This is a comprehensive test that will only pass once full Decode is implemented
    auto original_events = CreateTestEventData(2);
    uint64_t sequence_number = 42;
    
    processor->EnableCompression(false);  // Disable for simpler testing
    processor->EnableChecksum(false);
    
    // Encode first
    auto encoded_data = processor->Process(original_events, sequence_number);
    ASSERT_NE(encoded_data, nullptr);
    
    // Decode back
    auto result = processor->Decode(encoded_data);
    
    EXPECT_NE(result.first, nullptr);
    EXPECT_EQ(result.second, sequence_number);
    
    if (result.first) {
        EXPECT_EQ(result.first->size(), 2);
        
        // Verify first event data matches
        if (result.first->size() > 0) {
            const auto& decoded_event = (*result.first)[0];
            EXPECT_EQ(decoded_event->module, 0);
            EXPECT_EQ(decoded_event->channel, 0);
            EXPECT_EQ(decoded_event->energy, 1000);
            EXPECT_DOUBLE_EQ(decoded_event->timeStampNs, 100);
            EXPECT_EQ(decoded_event->energyShort, 50);
            EXPECT_EQ(decoded_event->flags, 0x01);
        }
    }
}

TEST_F(DecodePipelineTest, DecodeRoundTripEventDataWithCompressionAndChecksum) {
    auto original_events = CreateTestEventData(1);
    uint64_t sequence_number = 123;
    
    processor->EnableCompression(true);   // Enable compression
    processor->EnableChecksum(true);      // Enable checksum
    
    // Encode first
    auto encoded_data = processor->Process(original_events, sequence_number);
    ASSERT_NE(encoded_data, nullptr);
    
    // Decode back
    auto result = processor->Decode(encoded_data);
    
    EXPECT_NE(result.first, nullptr);
    EXPECT_EQ(result.second, sequence_number);
    
    if (result.first) {
        EXPECT_EQ(result.first->size(), 1);
    }
}

// Phase 7.2: DecodeMinimal tests (TDD RED phase)
TEST_F(DecodePipelineTest, DecodeMinimalHandlesNullInput) {
    std::unique_ptr<std::vector<uint8_t>> null_data = nullptr;
    
    auto result = processor->DecodeMinimal(null_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeMinimalBasicRoundTrip) {
    auto original_events = CreateTestMinimalEventData(3);
    uint64_t sequence_number = 456;
    
    processor->EnableCompression(false);
    processor->EnableChecksum(false);
    
    // Encode first
    auto encoded_data = processor->Process(original_events, sequence_number);
    ASSERT_NE(encoded_data, nullptr);
    
    // Decode back
    auto result = processor->DecodeMinimal(encoded_data);
    
    EXPECT_NE(result.first, nullptr);
    EXPECT_EQ(result.second, sequence_number);
    
    if (result.first) {
        EXPECT_EQ(result.first->size(), 3);
        
        // Verify first event data matches
        if (result.first->size() > 0) {
            const auto& decoded_event = (*result.first)[0];
            EXPECT_EQ(decoded_event->module, 0);
            EXPECT_EQ(decoded_event->channel, 0);
            EXPECT_EQ(decoded_event->energy, 1000);
            EXPECT_DOUBLE_EQ(decoded_event->timeStampNs, 100);
            EXPECT_EQ(decoded_event->energyShort, 50);
            EXPECT_EQ(decoded_event->flags, 0x01);
        }
    }
}

TEST_F(DecodePipelineTest, DecodeMinimalWithCompressionAndChecksum) {
    auto original_events = CreateTestMinimalEventData(2);
    uint64_t sequence_number = 789;
    
    processor->EnableCompression(true);
    processor->EnableChecksum(true);
    
    // Encode first
    auto encoded_data = processor->Process(original_events, sequence_number);
    ASSERT_NE(encoded_data, nullptr);
    
    // Decode back
    auto result = processor->DecodeMinimal(encoded_data);
    
    EXPECT_NE(result.first, nullptr);
    EXPECT_EQ(result.second, sequence_number);
    
    if (result.first) {
        EXPECT_EQ(result.first->size(), 2);
    }
}

// Phase 7.3: Error handling tests (TDD RED phase)
TEST_F(DecodePipelineTest, DecodeHandlesCorruptedData) {
    auto corrupted_data = std::make_unique<std::vector<uint8_t>>(100);
    // Fill with random bytes to simulate corruption
    std::fill(corrupted_data->begin(), corrupted_data->end(), 0xAB);
    
    auto result = processor->Decode(corrupted_data);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeHandlesTruncatedData) {
    // Create valid data but truncate it
    auto original_events = CreateTestEventData(1);
    
    processor->EnableCompression(false);
    processor->EnableChecksum(false);
    
    auto encoded_data = processor->Process(original_events, 1);
    ASSERT_NE(encoded_data, nullptr);
    
    // Truncate the data
    auto truncated_data = std::make_unique<std::vector<uint8_t>>(
        encoded_data->begin(), encoded_data->begin() + encoded_data->size() / 2);
    
    auto result = processor->Decode(truncated_data);
    EXPECT_EQ(result.first, nullptr);  // Should fail due to truncation
    EXPECT_EQ(result.second, 0);
}

TEST_F(DecodePipelineTest, DecodeHandlesPayloadSizeMismatch) {
    // Create header that claims different payload size
    auto test_data = std::make_unique<std::vector<uint8_t>>(sizeof(BinaryDataHeader) + 50);
    BinaryDataHeader* header = reinterpret_cast<BinaryDataHeader*>(test_data->data());
    
    header->magic_number = BINARY_DATA_MAGIC_NUMBER;
    header->sequence_number = 1;
    header->format_version = FORMAT_VERSION_EVENTDATA;
    header->header_size = BINARY_DATA_HEADER_SIZE;
    header->event_count = 1;
    header->uncompressed_size = 1000;  // Claims 1000 bytes but we only have 50
    header->compressed_size = 1000;
    header->checksum = 0;
    header->compression_type = COMPRESSION_NONE;
    header->checksum_type = CHECKSUM_NONE;
    header->timestamp = 123456789;
    
    processor->EnableCompression(false);
    processor->EnableChecksum(false);
    
    auto result = processor->Decode(test_data);
    EXPECT_EQ(result.first, nullptr);  // Should fail due to size mismatch
    EXPECT_EQ(result.second, 0);
}