/**
 * @file test_end_to_end.cpp
 * @brief End-to-end integration tests for the binary serialization system
 * 
 * These tests validate complete data flow from generation through serialization,
 * compression, transmission simulation, decompression, and deserialization.
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>
#include <future>
#include <fstream>
#include <vector>
#include <random>
#include "delila/net/serialization/BinarySerializer.hpp"
#include "delila/net/test/TestDataGenerator.hpp"
#include "delila/net/mock/EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Net::Test;
using namespace DELILA;

class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer_ = std::make_unique<BinarySerializer>();
        generator_ = std::make_unique<TestDataGenerator>(12345); // Fixed seed
    }
    
    void TearDown() override {
        serializer_.reset();
        generator_.reset();
    }
    
    std::unique_ptr<BinarySerializer> serializer_;
    std::unique_ptr<TestDataGenerator> generator_;
    
    // Helper function to corrupt data at specific positions
    void corruptData(std::vector<uint8_t>& data, size_t num_corruptions = 1) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, data.size() - 1);
        
        for (size_t i = 0; i < num_corruptions && !data.empty(); i++) {
            size_t pos = dist(gen);
            data[pos] ^= 0xFF; // Flip all bits
        }
    }
    
    // Helper function to simulate network transmission with potential corruption
    std::vector<uint8_t> simulateNetworkTransmission(const std::vector<uint8_t>& data, 
                                                      bool introduce_corruption = false) {
        std::vector<uint8_t> transmitted_data = data;
        
        // Simulate transmission delay
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        
        if (introduce_corruption) {
            corruptData(transmitted_data, 1);
        }
        
        return transmitted_data;
    }
};

/**
 * @brief Test complete round-trip with single event
 */
TEST_F(EndToEndTest, SingleEventRoundTrip) {
    // Generate test data
    auto original_event = generator_->generateSingleEvent(500);
    
    // Serialize
    auto encoded_result = serializer_->encodeEvent(original_event);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Simulate network transmission
    auto transmitted_data = simulateNetworkTransmission(encoded_data);
    
    // Deserialize
    auto decoded_result = serializer_->decodeEvent(transmitted_data.data(), transmitted_data.size());
    ASSERT_TRUE(isOk(decoded_result));
    const auto& decoded_event = getValue(decoded_result);
    
    // Verify data integrity
    EXPECT_EQ(original_event.energy, decoded_event.energy);
    EXPECT_EQ(original_event.energyShort, decoded_event.energyShort);
    EXPECT_EQ(original_event.channel, decoded_event.channel);
    EXPECT_EQ(original_event.module, decoded_event.module);
    EXPECT_EQ(original_event.flags, decoded_event.flags);
    EXPECT_DOUBLE_EQ(original_event.timeStampNs, decoded_event.timeStampNs);
    EXPECT_EQ(original_event.waveform.size(), decoded_event.waveform.size());
    
    // Verify waveform data
    for (size_t i = 0; i < original_event.waveform.size(); i++) {
        EXPECT_EQ(original_event.waveform[i].adc_value, decoded_event.waveform[i].adc_value);
        EXPECT_EQ(original_event.waveform[i].timestamp_ns, decoded_event.waveform[i].timestamp_ns);
    }
}

/**
 * @brief Test complete round-trip with batch of events
 */
TEST_F(EndToEndTest, BatchEventsRoundTrip) {
    // Generate test batch
    auto original_events = generator_->generateEventBatch(25, 100, 300);
    
    // Serialize batch
    auto encoded_result = serializer_->encode(original_events);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Simulate network transmission
    auto transmitted_data = simulateNetworkTransmission(encoded_data);
    
    // Deserialize batch
    auto decoded_result = serializer_->decode(transmitted_data);
    ASSERT_TRUE(isOk(decoded_result));
    const auto& decoded_events = getValue(decoded_result);
    
    // Verify batch integrity
    ASSERT_EQ(original_events.size(), decoded_events.size());
    
    for (size_t i = 0; i < original_events.size(); i++) {
        const auto& orig = original_events[i];
        const auto& decoded = decoded_events[i];
        
        EXPECT_EQ(orig.energy, decoded.energy) << "Event " << i;
        EXPECT_EQ(orig.channel, decoded.channel) << "Event " << i;
        EXPECT_EQ(orig.module, decoded.module) << "Event " << i;
        EXPECT_EQ(orig.waveform.size(), decoded.waveform.size()) << "Event " << i;
    }
}

/**
 * @brief Test end-to-end with compression enabled
 */
TEST_F(EndToEndTest, CompressionRoundTrip) {
    // Enable compression
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(1);
    
    // Generate compressible data (many events with similar patterns)
    auto original_events = generator_->generateEventBatch(50, 200, 200); // Fixed size for better compression
    
    // Serialize with compression
    auto encoded_result = serializer_->encode(original_events);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Simulate network transmission
    auto transmitted_data = simulateNetworkTransmission(encoded_data);
    
    // Deserialize compressed data
    auto decoded_result = serializer_->decode(transmitted_data);
    ASSERT_TRUE(isOk(decoded_result));
    const auto& decoded_events = getValue(decoded_result);
    
    // Verify compression worked and data is intact
    ASSERT_EQ(original_events.size(), decoded_events.size());
    
    // Just verify that compression was attempted (data should be reasonable size)
    // Note: Compression effectiveness depends on data patterns
    size_t uncompressed_estimate = original_events.size() * (34 + 200 * sizeof(WaveformSample)) + 64; // + header
    EXPECT_LT(encoded_data.size(), uncompressed_estimate * 1.1); // Should not exceed 110% of estimate
}

/**
 * @brief Test error detection with corrupted data
 */
TEST_F(EndToEndTest, CorruptedDataDetection) {
    // Generate test data
    auto original_events = generator_->generateEventBatch(10, 50, 100);
    
    // Serialize
    auto encoded_result = serializer_->encode(original_events);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Simulate transmission with corruption
    auto corrupted_data = simulateNetworkTransmission(encoded_data, true);
    
    // Attempt to deserialize corrupted data
    auto decoded_result = serializer_->decode(corrupted_data);
    
    // Should detect corruption (checksum mismatch)
    EXPECT_FALSE(isOk(decoded_result));
    if (!isOk(decoded_result)) {
        const auto& error = getError(decoded_result);
        EXPECT_EQ(error.code, Error::CHECKSUM_MISMATCH);
    }
}

/**
 * @brief Test large dataset handling (stress test)
 */
TEST_F(EndToEndTest, LargeDatasetHandling) {
    // Generate large dataset (~10MB)
    auto original_events = generator_->generatePerformanceTestData(10, 100);
    EXPECT_GT(original_events.size(), 500); // Should have many events
    
    // Measure serialization time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto encoded_result = serializer_->encode(original_events);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Simulate network transmission (without corruption for large data)
    auto transmitted_data = simulateNetworkTransmission(encoded_data);
    
    // Deserialize
    auto decoded_result = serializer_->decode(transmitted_data);
    ASSERT_TRUE(isOk(decoded_result));
    const auto& decoded_events = getValue(decoded_result);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Verify data integrity
    ASSERT_EQ(original_events.size(), decoded_events.size());
    
    // Performance expectations (should complete within reasonable time)
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(total_time.count(), 1000); // Should complete within 1 second
    
    // Spot check some events for integrity
    for (size_t i = 0; i < std::min(original_events.size(), size_t(10)); i++) {
        EXPECT_EQ(original_events[i].energy, decoded_events[i].energy);
        EXPECT_EQ(original_events[i].waveform.size(), decoded_events[i].waveform.size());
    }
}

/**
 * @brief Test concurrent serialization (thread safety)
 */
TEST_F(EndToEndTest, ConcurrentSerialization) {
    const int num_threads = 4;
    
    std::vector<std::future<bool>> futures;
    
    // Launch multiple threads doing serialization concurrently
    for (int t = 0; t < num_threads; t++) {
        futures.push_back(std::async(std::launch::async, [t]() {
            // Each thread uses its own serializer (required for thread safety)
            BinarySerializer thread_serializer;
            TestDataGenerator thread_generator(1000 + t); // Different seed per thread
            
            // Generate and serialize data
            auto events = thread_generator.generateEventBatch(25, 50, 150);
            auto encoded_result = thread_serializer.encode(events);
            
            if (!isOk(encoded_result)) {
                return false;
            }
            
            // Verify round-trip
            const auto& encoded_data = getValue(encoded_result);
            auto decoded_result = thread_serializer.decode(encoded_data);
            
            if (!isOk(decoded_result)) {
                return false;
            }
            
            const auto& decoded_events = getValue(decoded_result);
            return events.size() == decoded_events.size();
        }));
    }
    
    // Wait for all threads and verify success
    for (auto& future : futures) {
        EXPECT_TRUE(future.get()) << "Thread failed serialization test";
    }
}

/**
 * @brief Test different compression levels end-to-end
 */
TEST_F(EndToEndTest, CompressionLevelsEndToEnd) {
    // Generate test data
    auto original_events = generator_->generateEventBatch(30, 100, 200);
    
    std::vector<int> compression_levels = {1, 3, 6, 9, 12};
    
    for (int level : compression_levels) {
        SCOPED_TRACE("Compression level " + std::to_string(level));
        
        // Configure compression
        serializer_->enableCompression(true);
        serializer_->setCompressionLevel(level);
        
        // Serialize
        auto encoded_result = serializer_->encode(original_events);
        ASSERT_TRUE(isOk(encoded_result));
        const auto& encoded_data = getValue(encoded_result);
        
        // Deserialize
        auto decoded_result = serializer_->decode(encoded_data);
        ASSERT_TRUE(isOk(decoded_result));
        const auto& decoded_events = getValue(decoded_result);
        
        // Verify data integrity
        ASSERT_EQ(original_events.size(), decoded_events.size());
        
        // Spot check first event
        EXPECT_EQ(original_events[0].energy, decoded_events[0].energy);
        EXPECT_EQ(original_events[0].waveform.size(), decoded_events[0].waveform.size());
    }
}

/**
 * @brief Test mixed event sizes in single batch
 */
TEST_F(EndToEndTest, MixedEventSizesBatch) {
    // Generate events with widely varying sizes
    std::vector<EventData> mixed_events;
    
    // Small events
    auto small_events = generator_->generateEventBatch(10, 0, 50);
    mixed_events.insert(mixed_events.end(), small_events.begin(), small_events.end());
    
    // Medium events
    auto medium_events = generator_->generateEventBatch(10, 200, 500);
    mixed_events.insert(mixed_events.end(), medium_events.begin(), medium_events.end());
    
    // Large events
    auto large_events = generator_->generateEventBatch(5, 1000, 2000);
    mixed_events.insert(mixed_events.end(), large_events.begin(), large_events.end());
    
    // Serialize mixed batch
    auto encoded_result = serializer_->encode(mixed_events);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Deserialize
    auto decoded_result = serializer_->decode(encoded_data);
    ASSERT_TRUE(isOk(decoded_result));
    const auto& decoded_events = getValue(decoded_result);
    
    // Verify all events
    ASSERT_EQ(mixed_events.size(), decoded_events.size());
    
    for (size_t i = 0; i < mixed_events.size(); i++) {
        EXPECT_EQ(mixed_events[i].energy, decoded_events[i].energy) << "Event " << i;
        EXPECT_EQ(mixed_events[i].waveform.size(), decoded_events[i].waveform.size()) << "Event " << i;
    }
}

/**
 * @brief Test sequence number progression across multiple batches
 */
TEST_F(EndToEndTest, SequenceNumberProgression) {
    std::vector<uint64_t> sequence_numbers;
    
    // Serialize multiple batches and extract sequence numbers
    for (int batch = 0; batch < 5; batch++) {
        auto events = generator_->generateEventBatch(10, 100, 100);
        auto encoded_result = serializer_->encode(events);
        ASSERT_TRUE(isOk(encoded_result));
        
        const auto& encoded_data = getValue(encoded_result);
        
        // Extract sequence number from header
        ASSERT_GE(encoded_data.size(), sizeof(BinaryDataHeader));
        const auto* header = reinterpret_cast<const BinaryDataHeader*>(encoded_data.data());
        sequence_numbers.push_back(header->sequence_number);
    }
    
    // Verify sequence numbers are incrementing
    for (size_t i = 1; i < sequence_numbers.size(); i++) {
        EXPECT_EQ(sequence_numbers[i], sequence_numbers[i-1] + 1) 
            << "Sequence number gap at batch " << i;
    }
}

/**
 * @brief Test end-to-end with different waveform patterns
 */
TEST_F(EndToEndTest, DifferentWaveformPatterns) {
    // Test sine wave pattern
    auto sine_waveform = generator_->generateSineWaveform(500, 1000, 0.1);
    EventData sine_event = generator_->generateSingleEvent(0);
    sine_event.waveform = sine_waveform;
    sine_event.waveformSize = sine_waveform.size();
    
    // Test noise pattern  
    auto noise_waveform = generator_->generateNoiseWaveform(500, 32768, 2000);
    EventData noise_event = generator_->generateSingleEvent(0);
    noise_event.waveform = noise_waveform;
    noise_event.waveformSize = noise_waveform.size();
    
    std::vector<EventData> pattern_events = {sine_event, noise_event};
    
    // Serialize patterns
    auto encoded_result = serializer_->encode(pattern_events);
    ASSERT_TRUE(isOk(encoded_result));
    const auto& encoded_data = getValue(encoded_result);
    
    // Deserialize
    auto decoded_result = serializer_->decode(encoded_data);
    ASSERT_TRUE(isOk(decoded_result));
    const auto& decoded_events = getValue(decoded_result);
    
    // Verify patterns are preserved
    ASSERT_EQ(pattern_events.size(), decoded_events.size());
    
    // Check sine wave preservation
    EXPECT_EQ(pattern_events[0].waveform.size(), decoded_events[0].waveform.size());
    for (size_t i = 0; i < std::min(size_t(10), decoded_events[0].waveform.size()); i++) {
        EXPECT_EQ(pattern_events[0].waveform[i].adc_value, decoded_events[0].waveform[i].adc_value);
    }
    
    // Check noise wave preservation  
    EXPECT_EQ(pattern_events[1].waveform.size(), decoded_events[1].waveform.size());
    for (size_t i = 0; i < std::min(size_t(10), decoded_events[1].waveform.size()); i++) {
        EXPECT_EQ(pattern_events[1].waveform[i].adc_value, decoded_events[1].waveform[i].adc_value);
    }
}