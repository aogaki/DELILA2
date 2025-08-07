#include <gtest/gtest.h>
#include "../../lib/net/include/Serializer.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::MinimalEventData;

class MinimalDataTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer = std::make_unique<Serializer>();
    }
    
    std::unique_ptr<Serializer> serializer;
    
    // Helper function to create test MinimalEventData
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEvents(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        events->reserve(count);
        
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

// Test end-to-end data flow simulation (encode -> "transport" -> decode)
TEST_F(MinimalDataTransportTest, EndToEndDataFlow) {
    // Create test data (simulating digitizer output)
    auto original_events = CreateTestMinimalEvents(100);
    
    // Store original data for comparison
    std::vector<MinimalEventData> expected_data;
    for (const auto& event : *original_events) {
        expected_data.push_back(*event);
    }
    
    // Step 1: Encode (simulating sender side)
    auto encoded_data = serializer->Encode(original_events, 12345);
    ASSERT_NE(encoded_data, nullptr);
    
    // Simulate network transport (data would be sent over ZMQ here)
    // For now, we just pass the encoded data directly
    
    // Step 2: Decode (simulating receiver side)
    auto [received_events, sequence] = serializer->DecodeMinimalEventData(encoded_data);
    
    // Verify successful transport
    ASSERT_NE(received_events, nullptr);
    ASSERT_EQ(received_events->size(), 100);
    EXPECT_EQ(sequence, 12345);
    
    // Verify data integrity after "transport"
    for (size_t i = 0; i < 100; ++i) {
        const auto& received = *(*received_events)[i];
        const auto& expected = expected_data[i];
        
        EXPECT_EQ(received.module, expected.module) << "Event " << i << " module mismatch";
        EXPECT_EQ(received.channel, expected.channel) << "Event " << i << " channel mismatch";
        EXPECT_EQ(received.timeStampNs, expected.timeStampNs) << "Event " << i << " timestamp mismatch";
        EXPECT_EQ(received.energy, expected.energy) << "Event " << i << " energy mismatch";
        EXPECT_EQ(received.energyShort, expected.energyShort) << "Event " << i << " energyShort mismatch";
        EXPECT_EQ(received.flags, expected.flags) << "Event " << i << " flags mismatch";
    }
}

// Test high-throughput data flow
TEST_F(MinimalDataTransportTest, HighThroughputDataFlow) {
    const size_t LARGE_EVENT_COUNT = 10000;
    
    // Create large dataset
    auto original_events = CreateTestMinimalEvents(LARGE_EVENT_COUNT);
    
    // Encode
    auto start_encode = std::chrono::high_resolution_clock::now();
    auto encoded_data = serializer->Encode(original_events, 99999);
    auto end_encode = std::chrono::high_resolution_clock::now();
    
    ASSERT_NE(encoded_data, nullptr);
    
    // Decode
    auto start_decode = std::chrono::high_resolution_clock::now();
    auto [received_events, sequence] = serializer->DecodeMinimalEventData(encoded_data);
    auto end_decode = std::chrono::high_resolution_clock::now();
    
    // Verify successful processing
    ASSERT_NE(received_events, nullptr);
    EXPECT_EQ(received_events->size(), LARGE_EVENT_COUNT);
    EXPECT_EQ(sequence, 99999);
    
    // Calculate throughput metrics
    auto encode_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_encode - start_encode);
    auto decode_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_decode - start_decode);
    
    double encode_rate = static_cast<double>(LARGE_EVENT_COUNT) / encode_duration.count() * 1e6; // events/sec
    double decode_rate = static_cast<double>(LARGE_EVENT_COUNT) / decode_duration.count() * 1e6; // events/sec
    
    // Log performance for review (not assertions since performance varies by system)
    std::cout << "Encode rate: " << encode_rate << " events/sec" << std::endl;
    std::cout << "Decode rate: " << decode_rate << " events/sec" << std::endl;
    
    // Sanity check - should be able to process at least 1000 events/sec
    EXPECT_GT(encode_rate, 1000.0);
    EXPECT_GT(decode_rate, 1000.0);
}

// Test transport with various data sizes  
TEST_F(MinimalDataTransportTest, VariableDataSizes) {
    std::vector<size_t> test_sizes = {1, 10, 100, 1000, 5000};
    
    for (size_t size : test_sizes) {
        SCOPED_TRACE("Testing with " + std::to_string(size) + " events");
        
        // Create test data
        auto original_events = CreateTestMinimalEvents(size);
        
        // Transport simulation
        auto encoded_data = serializer->Encode(original_events, size); // Use size as sequence
        ASSERT_NE(encoded_data, nullptr);
        
        auto [received_events, sequence] = serializer->DecodeMinimalEventData(encoded_data);
        ASSERT_NE(received_events, nullptr);
        
        // Verify size and sequence
        EXPECT_EQ(received_events->size(), size);
        EXPECT_EQ(sequence, size);
        
        // Verify data size efficiency
        size_t expected_payload_size = size * sizeof(MinimalEventData);
        size_t actual_total_size = encoded_data->size();
        size_t header_size = sizeof(BinaryDataHeader);
        
        EXPECT_EQ(actual_total_size, header_size + expected_payload_size);
    }
}

// Test error conditions in transport
TEST_F(MinimalDataTransportTest, TransportErrorHandling) {
    // Test with null data
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> null_events = nullptr;
    auto encoded_null = serializer->Encode(null_events, 1);
    EXPECT_EQ(encoded_null, nullptr);
    
    // Test with empty data
    auto empty_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    auto encoded_empty = serializer->Encode(empty_events, 2);
    ASSERT_NE(encoded_empty, nullptr);
    
    auto [decoded_empty, seq_empty] = serializer->DecodeMinimalEventData(encoded_empty);
    ASSERT_NE(decoded_empty, nullptr);
    EXPECT_EQ(decoded_empty->size(), 0);
    EXPECT_EQ(seq_empty, 2);
}

// Test sequence number handling across multiple transports
TEST_F(MinimalDataTransportTest, SequenceNumberHandling) {
    uint64_t sequence_numbers[] = {0, 1, 100, 65535, 4294967295ULL, 18446744073709551615ULL}; // Various edge cases
    
    for (uint64_t seq_num : sequence_numbers) {
        SCOPED_TRACE("Testing sequence number: " + std::to_string(seq_num));
        
        auto events = CreateTestMinimalEvents(5);
        auto encoded = serializer->Encode(events, seq_num);
        ASSERT_NE(encoded, nullptr);
        
        auto [decoded, received_seq] = serializer->DecodeMinimalEventData(encoded);
        ASSERT_NE(decoded, nullptr);
        EXPECT_EQ(received_seq, seq_num);
    }
}

// Test simulated data corruption during transport
TEST_F(MinimalDataTransportTest, DataCorruptionHandling) {
    auto original_events = CreateTestMinimalEvents(10);
    auto encoded_data = serializer->Encode(original_events, 42);
    ASSERT_NE(encoded_data, nullptr);
    
    // Simulate data corruption by modifying payload
    if (encoded_data->size() > sizeof(BinaryDataHeader) + 10) {
        // Corrupt some bytes in the payload (after header)
        (*encoded_data)[sizeof(BinaryDataHeader) + 5] ^= 0xFF;
        (*encoded_data)[sizeof(BinaryDataHeader) + 10] ^= 0xFF;
    }
    
    // Try to decode corrupted data
    auto [corrupted_events, seq] = serializer->DecodeMinimalEventData(encoded_data);
    
    // Depending on checksum settings, this might fail or succeed with corrupted data
    if (corrupted_events == nullptr) {
        // Checksum validation caught the corruption (good!)
        EXPECT_EQ(seq, 0);
        std::cout << "Checksum validation successfully detected data corruption" << std::endl;
    } else {
        // No checksum validation - data decoded but may be corrupted
        EXPECT_EQ(corrupted_events->size(), 10);
        EXPECT_EQ(seq, 42);
        std::cout << "Data decoded despite corruption - checksum validation may be disabled" << std::endl;
    }
}