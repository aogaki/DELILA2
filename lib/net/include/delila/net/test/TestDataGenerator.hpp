/**
 * @file TestDataGenerator.hpp
 * @brief Test data generation utilities for BinarySerializer testing
 * 
 * Provides various methods to generate realistic test data for
 * EventData structures with configurable parameters.
 */

#pragma once

#include <vector>
#include <random>
#include <cmath>
#include "delila/net/mock/EventData.hpp"
#include "delila/net/utils/Platform.hpp"

namespace DELILA::Net::Test {

/**
 * @brief Test data generator for EventData structures
 * 
 * Provides methods to generate single events, batches, and performance
 * test data with realistic waveforms and sequential timestamps.
 */
class TestDataGenerator {
public:
    /**
     * @brief Constructor with optional seed
     * @param seed Random seed (0 = use random seed)
     */
    explicit TestDataGenerator(uint32_t seed = 0);
    
    /**
     * @brief Generate a single EventData with specified waveform size
     * @param waveform_size Number of waveform samples
     * @return Generated EventData
     */
    DELILA::EventData generateSingleEvent(uint32_t waveform_size);
    
    /**
     * @brief Generate a batch of EventData with varying waveform sizes
     * @param count Number of events to generate
     * @param min_waveform_size Minimum waveform size
     * @param max_waveform_size Maximum waveform size
     * @return Vector of generated EventData
     */
    std::vector<DELILA::EventData> generateEventBatch(
        size_t count, 
        uint32_t min_waveform_size, 
        uint32_t max_waveform_size
    );
    
    /**
     * @brief Generate performance test data targeting specific total size
     * @param target_size_mb Target total size in megabytes
     * @param events_per_mb Approximate number of events per MB
     * @return Vector of generated EventData
     */
    std::vector<DELILA::EventData> generatePerformanceTestData(
        size_t target_size_mb,
        size_t events_per_mb = 100
    );
    
    /**
     * @brief Generate sine wave waveform samples
     * @param num_samples Number of samples to generate
     * @param amplitude Amplitude of sine wave (0-65535)
     * @param frequency Frequency factor (cycles per sample set)
     * @param base_timestamp Starting timestamp in nanoseconds
     * @param sample_interval Interval between samples in nanoseconds
     * @return Vector of WaveformSample
     */
    std::vector<DELILA::WaveformSample> generateSineWaveform(
        uint32_t num_samples,
        uint16_t amplitude = 32767,
        double frequency = 0.1,
        uint64_t base_timestamp = 0,
        uint64_t sample_interval = 10
    );
    
    /**
     * @brief Generate random noise waveform samples
     * @param num_samples Number of samples to generate
     * @param base_value Base ADC value (noise will be added to this)
     * @param noise_amplitude Maximum noise amplitude
     * @param base_timestamp Starting timestamp in nanoseconds
     * @param sample_interval Interval between samples in nanoseconds
     * @return Vector of WaveformSample
     */
    std::vector<DELILA::WaveformSample> generateNoiseWaveform(
        uint32_t num_samples,
        uint16_t base_value = 32768,
        uint16_t noise_amplitude = 1000,
        uint64_t base_timestamp = 0,
        uint64_t sample_interval = 10
    );
    
    /**
     * @brief Reset generator with new seed
     * @param seed New random seed (0 = use random seed)
     */
    void setSeed(uint32_t seed);
    
    /**
     * @brief Get current generator seed
     * @return Current seed value
     */
    uint32_t getCurrentSeed() const { return current_seed_; }
    
    /**
     * @brief Generate random timestamp near current time
     * @return Timestamp in nanoseconds
     */
    uint64_t generateTimestamp();
    
    /**
     * @brief Calculate approximate size of an EventData
     * @param event EventData to calculate size for
     * @return Approximate size in bytes
     */
    size_t calculateEventSize(const DELILA::EventData& event) const;

private:
    std::mt19937 random_engine_;                      ///< Random number generator
    std::uniform_int_distribution<uint32_t> int_dist_; ///< Integer distribution
    std::uniform_real_distribution<double> real_dist_; ///< Real distribution
    std::atomic<uint32_t> event_counter_;             ///< Event number counter
    uint32_t current_seed_;                           ///< Current seed value
};

} // namespace DELILA::Net::Test