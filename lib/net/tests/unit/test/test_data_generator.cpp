/**
 * @file test_data_generator.cpp
 * @brief Unit tests for TestDataGenerator class
 */

#include <gtest/gtest.h>
#include <memory>
#include <algorithm>
#include <cmath>
#include "delila/net/test/TestDataGenerator.hpp"
#include "delila/net/mock/EventData.hpp"

using namespace DELILA::Net::Test;
using namespace DELILA;

class TestDataGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        generator_ = std::make_unique<TestDataGenerator>(42); // Fixed seed for reproducibility
    }
    
    void TearDown() override {
        generator_.reset();
    }
    
    std::unique_ptr<TestDataGenerator> generator_;
};

/**
 * @brief Test single event generation with default parameters
 */
TEST_F(TestDataGeneratorTest, SingleEventGenerationDefault) {
    auto event = generator_->generateSingleEvent(100);
    
    // Verify event has reasonable values
    EXPECT_LE(event.channel, 63);        // Channel should be within reasonable range
    EXPECT_LE(event.module, 15);         // Module should be within reasonable range
    EXPECT_GT(event.timeStampNs, 0);     // Timestamp should be positive
    EXPECT_EQ(event.waveformSize, event.waveform.size()); // Size consistency
}

/**
 * @brief Test single event generation with specific waveform size
 */
TEST_F(TestDataGeneratorTest, SingleEventGenerationWithSize) {
    const uint32_t waveform_size = 1000;
    auto event = generator_->generateSingleEvent(waveform_size);
    
    EXPECT_EQ(event.waveform.size(), waveform_size);
    EXPECT_EQ(event.waveformSize, waveform_size);
    
    // Verify waveform data is reasonable
    if (!event.waveform.empty()) {
        for (const auto& sample : event.waveform) {
            EXPECT_LE(sample.adc_value, 65535);  // 16-bit ADC value
            EXPECT_GT(sample.timestamp_ns, 0);   // Valid timestamp
        }
    }
}

/**
 * @brief Test single event generation with zero waveform size
 */
TEST_F(TestDataGeneratorTest, SingleEventGenerationEmptyWaveform) {
    auto event = generator_->generateSingleEvent(0);
    
    EXPECT_EQ(event.waveform.size(), 0);
    EXPECT_EQ(event.waveformSize, 0);
    EXPECT_GT(event.timeStampNs, 0);
}

/**
 * @brief Test batch event generation
 */
TEST_F(TestDataGeneratorTest, BatchEventGeneration) {
    const size_t batch_size = 50;
    const uint32_t min_waveform = 100;
    const uint32_t max_waveform = 500;
    
    auto events = generator_->generateEventBatch(batch_size, min_waveform, max_waveform);
    
    EXPECT_EQ(events.size(), batch_size);
    
    // Verify each event in batch
    for (size_t i = 0; i < events.size(); i++) {
        const auto& event = events[i];
        
        // Check waveform size is within range
        EXPECT_GE(event.waveform.size(), min_waveform);
        EXPECT_LE(event.waveform.size(), max_waveform);
        
        // Check module and channel are set for identification
        EXPECT_EQ(event.module, static_cast<uint8_t>(i % 16));   // Module 0-15
        EXPECT_EQ(event.channel, static_cast<uint8_t>(i % 64));  // Channel 0-63
        
        // Check consistency
        EXPECT_EQ(event.waveformSize, event.waveform.size());
    }
}

/**
 * @brief Test batch generation with single waveform size
 */
TEST_F(TestDataGeneratorTest, BatchEventGenerationFixedSize) {
    const size_t batch_size = 10;
    const uint32_t waveform_size = 200;
    
    auto events = generator_->generateEventBatch(batch_size, waveform_size, waveform_size);
    
    EXPECT_EQ(events.size(), batch_size);
    
    for (const auto& event : events) {
        EXPECT_EQ(event.waveform.size(), waveform_size);
        EXPECT_EQ(event.waveformSize, waveform_size);
    }
}

/**
 * @brief Test performance test data generation
 */
TEST_F(TestDataGeneratorTest, PerformanceTestDataGeneration) {
    const size_t target_mb = 5;
    const size_t events_per_mb = 100;
    
    auto events = generator_->generatePerformanceTestData(target_mb, events_per_mb);
    
    // Should generate approximately events_per_mb * target_mb events
    size_t expected_events = target_mb * events_per_mb;
    EXPECT_EQ(events.size(), expected_events);
    
    // Calculate total size and verify it's approximately target_mb
    size_t total_size = 0;
    for (const auto& event : events) {
        total_size += generator_->calculateEventSize(event);
    }
    
    size_t target_bytes = target_mb * 1024 * 1024;
    // Allow 20% tolerance for size calculation
    EXPECT_GE(total_size, target_bytes * 0.8);
    EXPECT_LE(total_size, target_bytes * 1.2);
}

/**
 * @brief Test sine wave waveform generation
 */
TEST_F(TestDataGeneratorTest, SineWaveWaveformGeneration) {
    const uint32_t num_samples = 1000;
    const uint16_t amplitude = 1000;
    const double frequency = 0.1; // Low frequency for clear pattern
    const uint64_t base_timestamp = 1000000;
    const uint64_t sample_interval = 10;
    
    auto waveform = generator_->generateSineWaveform(num_samples, amplitude, frequency, 
                                                    base_timestamp, sample_interval);
    
    EXPECT_EQ(waveform.size(), num_samples);
    
    // Verify sine wave properties
    for (size_t i = 0; i < waveform.size(); i++) {
        const auto& sample = waveform[i];
        
        // Check timestamp progression
        EXPECT_EQ(sample.timestamp_ns, base_timestamp + i * sample_interval);
        
        // Check ADC value is within reasonable range
        EXPECT_LE(sample.adc_value, 65535);
    }
    
    // Verify sine wave pattern by checking peak detection
    std::vector<uint16_t> values;
    for (const auto& sample : waveform) {
        values.push_back(sample.adc_value);
    }
    
    // Find min and max values
    auto min_val = *std::min_element(values.begin(), values.end());
    auto max_val = *std::max_element(values.begin(), values.end());
    
    // Should have variation due to sine wave
    EXPECT_GT(max_val - min_val, amplitude / 2); // At least half amplitude variation
}

/**
 * @brief Test noise waveform generation
 */
TEST_F(TestDataGeneratorTest, NoiseWaveformGeneration) {
    const uint32_t num_samples = 1000;
    const uint16_t base_value = 32768;
    const uint16_t noise_amplitude = 1000;
    const uint64_t base_timestamp = 2000000;
    const uint64_t sample_interval = 5;
    
    auto waveform = generator_->generateNoiseWaveform(num_samples, base_value, noise_amplitude,
                                                     base_timestamp, sample_interval);
    
    EXPECT_EQ(waveform.size(), num_samples);
    
    // Verify noise properties
    std::vector<uint16_t> values;
    for (size_t i = 0; i < waveform.size(); i++) {
        const auto& sample = waveform[i];
        
        // Check timestamp progression
        EXPECT_EQ(sample.timestamp_ns, base_timestamp + i * sample_interval);
        
        // Check ADC value is within noise range
        EXPECT_GE(sample.adc_value, base_value - noise_amplitude);
        EXPECT_LE(sample.adc_value, base_value + noise_amplitude);
        
        values.push_back(sample.adc_value);
    }
    
    // Verify randomness - calculate standard deviation
    double mean = 0;
    for (uint16_t val : values) {
        mean += val;
    }
    mean /= values.size();
    
    double variance = 0;
    for (uint16_t val : values) {
        variance += (val - mean) * (val - mean);
    }
    variance /= values.size();
    
    // Should have some variance due to noise
    EXPECT_GT(variance, 0);
    EXPECT_LT(std::abs(mean - base_value), noise_amplitude / 2); // Mean should be near base
}

/**
 * @brief Test seed functionality for reproducibility
 */
TEST_F(TestDataGeneratorTest, SeedReproducibility) {
    const uint32_t seed = 123456;
    
    // Generate data with first generator
    TestDataGenerator gen1(seed);
    auto event1 = gen1.generateSingleEvent(100);
    auto batch1 = gen1.generateEventBatch(5, 50, 100);
    
    // Generate data with second generator using same seed
    TestDataGenerator gen2(seed);
    auto event2 = gen2.generateSingleEvent(100);
    auto batch2 = gen2.generateEventBatch(5, 50, 100);
    
    // Single events should be identical
    EXPECT_EQ(event1.energy, event2.energy);
    EXPECT_EQ(event1.energyShort, event2.energyShort);
    EXPECT_EQ(event1.channel, event2.channel);
    EXPECT_EQ(event1.module, event2.module);
    EXPECT_EQ(event1.waveform.size(), event2.waveform.size());
    
    // Batches should be identical
    EXPECT_EQ(batch1.size(), batch2.size());
    for (size_t i = 0; i < batch1.size(); i++) {
        EXPECT_EQ(batch1[i].energy, batch2[i].energy);
        EXPECT_EQ(batch1[i].waveform.size(), batch2[i].waveform.size());
    }
}

/**
 * @brief Test setSeed functionality
 */
TEST_F(TestDataGeneratorTest, SetSeedFunctionality) {
    const uint32_t seed1 = 111;
    const uint32_t seed2 = 222;
    
    // Generate with first seed
    generator_->setSeed(seed1);
    auto event1 = generator_->generateSingleEvent(50);
    
    // Generate with second seed
    generator_->setSeed(seed2);
    auto event2 = generator_->generateSingleEvent(50);
    
    // Reset to first seed
    generator_->setSeed(seed1);
    auto event3 = generator_->generateSingleEvent(50);
    
    // Events 1 and 3 should be identical (same seed)
    EXPECT_EQ(event1.energy, event3.energy);
    EXPECT_EQ(event1.energyShort, event3.energyShort);
    EXPECT_EQ(event1.channel, event3.channel);
    
    // Event 2 should be different (different seed)
    EXPECT_TRUE(event1.energy != event2.energy || 
                event1.energyShort != event2.energyShort ||
                event1.channel != event2.channel);
}

/**
 * @brief Test timestamp generation
 */
TEST_F(TestDataGeneratorTest, TimestampGeneration) {
    uint64_t timestamp1 = generator_->generateTimestamp();
    uint64_t timestamp2 = generator_->generateTimestamp();
    
    // Timestamps should be positive
    EXPECT_GT(timestamp1, 0);
    EXPECT_GT(timestamp2, 0);
    
    // Timestamps should be in nanoseconds (reasonably large)
    EXPECT_GT(timestamp1, 1000000000ULL); // > 1 second in nanoseconds
    EXPECT_GT(timestamp2, 1000000000ULL);
}

/**
 * @brief Test event size calculation
 */
TEST_F(TestDataGeneratorTest, EventSizeCalculation) {
    auto event = generator_->generateSingleEvent(500);
    size_t calculated_size = generator_->calculateEventSize(event);
    
    // Size should be header (34 bytes) + waveform data
    size_t expected_size = 34 + event.waveform.size() * sizeof(DELILA::WaveformSample);
    EXPECT_EQ(calculated_size, expected_size);
    
    // Test with empty waveform
    auto empty_event = generator_->generateSingleEvent(0);
    size_t empty_size = generator_->calculateEventSize(empty_event);
    EXPECT_EQ(empty_size, 34); // Just header size
}

/**
 * @brief Test large data generation performance
 */
TEST_F(TestDataGeneratorTest, LargeDataGeneration) {
    const size_t large_batch = 1000;
    const uint32_t large_waveform = 10000;
    
    auto events = generator_->generateEventBatch(large_batch, large_waveform, large_waveform);
    
    EXPECT_EQ(events.size(), large_batch);
    
    // Calculate total memory usage
    size_t total_size = 0;
    for (const auto& event : events) {
        total_size += generator_->calculateEventSize(event);
    }
    
    // Should be approximately 100MB for this configuration (allow for header overhead)
    EXPECT_GT(total_size, 95 * 1024 * 1024);  // At least 95MB
    
    // Verify all events have expected waveform size
    for (const auto& event : events) {
        EXPECT_EQ(event.waveform.size(), large_waveform);
    }
}

/**
 * @brief Test field value ranges
 */
TEST_F(TestDataGeneratorTest, FieldValueRanges) {
    const size_t test_events = 100;
    auto events = generator_->generateEventBatch(test_events, 10, 100);
    
    for (const auto& event : events) {
        // Test field ranges
        EXPECT_LE(event.analogProbe1Type, 255);
        EXPECT_LE(event.analogProbe2Type, 255);
        EXPECT_LE(event.channel, 63);  // Should be < 64
        EXPECT_LE(event.digitalProbe1Type, 255);
        EXPECT_LE(event.digitalProbe2Type, 255);
        EXPECT_LE(event.digitalProbe3Type, 255);
        EXPECT_LE(event.digitalProbe4Type, 255);
        EXPECT_GE(event.downSampleFactor, 1);
        EXPECT_LE(event.downSampleFactor, 8);
        EXPECT_LE(event.energy, 65535);
        EXPECT_LE(event.energyShort, 65535);
        EXPECT_LE(event.module, 15);   // Should be < 16
        EXPECT_LE(event.timeResolution, 255);
        EXPECT_GT(event.timeStampNs, 0);
        EXPECT_EQ(event.waveformSize, event.waveform.size());
    }
}