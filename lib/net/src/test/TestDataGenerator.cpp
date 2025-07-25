/**
 * @file TestDataGenerator.cpp
 * @brief Implementation of TestDataGenerator class
 */

#include "delila/net/test/TestDataGenerator.hpp"
#include "delila/net/utils/Platform.hpp"
#include <random>
#include <cmath>

namespace DELILA::Net::Test {

TestDataGenerator::TestDataGenerator(uint32_t seed) 
    : random_engine_(seed == 0 ? std::random_device{}() : seed)
    , int_dist_(0, UINT32_MAX)
    , real_dist_(0.0, 1.0)
    , event_counter_(0)
    , current_seed_(seed)
{
}

DELILA::EventData TestDataGenerator::generateSingleEvent(uint32_t waveform_size) {
    DELILA::EventData event;
    
    // Generate random values for all fields
    event.analogProbe1Type = int_dist_(random_engine_) % 256;
    event.analogProbe2Type = int_dist_(random_engine_) % 256;
    event.channel = int_dist_(random_engine_) % 64;
    event.digitalProbe1Type = int_dist_(random_engine_) % 256;
    event.digitalProbe2Type = int_dist_(random_engine_) % 256;
    event.digitalProbe3Type = int_dist_(random_engine_) % 256;
    event.digitalProbe4Type = int_dist_(random_engine_) % 256;
    event.downSampleFactor = 1 + (int_dist_(random_engine_) % 8);
    event.energy = int_dist_(random_engine_) % 65536;
    event.energyShort = int_dist_(random_engine_) % 65536;
    event.flags = int_dist_(random_engine_);
    event.module = int_dist_(random_engine_) % 16;
    event.timeResolution = int_dist_(random_engine_) % 256;
    event.timeStampNs = real_dist_(random_engine_) * 1e12; // Large timestamp
    event.waveformSize = waveform_size;
    
    // Generate waveform samples
    event.waveform = generateNoiseWaveform(waveform_size, 32768, 1000, 
                                          static_cast<uint64_t>(event.timeStampNs), 10);
    
    return event;
}

std::vector<DELILA::EventData> TestDataGenerator::generateEventBatch(
    size_t count, 
    uint32_t min_waveform_size, 
    uint32_t max_waveform_size) {
    
    std::vector<DELILA::EventData> events;
    events.reserve(count);
    
    for (size_t i = 0; i < count; i++) {
        // Vary waveform size within range
        uint32_t waveform_size = min_waveform_size;
        if (max_waveform_size > min_waveform_size) {
            waveform_size = min_waveform_size + 
                           (int_dist_(random_engine_) % (max_waveform_size - min_waveform_size + 1));
        }
        
        events.push_back(generateSingleEvent(waveform_size));
        
        // Make events identifiable
        events.back().module = static_cast<uint8_t>(i % 16);  // Module 0-15
        events.back().channel = static_cast<uint8_t>(i % 64); // Channel 0-63
    }
    
    return events;
}

std::vector<DELILA::EventData> TestDataGenerator::generatePerformanceTestData(
    size_t target_size_mb,
    size_t events_per_mb) {
    
    // Calculate approximate event size for target
    size_t target_bytes = target_size_mb * 1024 * 1024;
    size_t total_events = target_size_mb * events_per_mb;
    
    // Calculate average waveform size needed
    size_t bytes_per_event = target_bytes / total_events;
    size_t header_size = 34; // SerializedEventDataHeader size
    size_t waveform_bytes = (bytes_per_event > header_size) ? bytes_per_event - header_size : 0;
    uint32_t waveform_samples = static_cast<uint32_t>(waveform_bytes / sizeof(DELILA::WaveformSample));
    
    // Generate events with calculated waveform size
    return generateEventBatch(total_events, waveform_samples, waveform_samples);
}

std::vector<DELILA::WaveformSample> TestDataGenerator::generateSineWaveform(
    uint32_t num_samples,
    uint16_t amplitude,
    double frequency,
    uint64_t base_timestamp,
    uint64_t sample_interval) {
    
    std::vector<DELILA::WaveformSample> samples;
    samples.reserve(num_samples);
    
    for (uint32_t i = 0; i < num_samples; i++) {
        DELILA::WaveformSample sample;
        
        // Generate sine wave with some noise
        double sine_value = std::sin(2.0 * M_PI * frequency * i / num_samples);
        double noise = (real_dist_(random_engine_) - 0.5) * 0.1; // 10% noise
        
        sample.adc_value = static_cast<uint16_t>(
            32768 + amplitude * (sine_value + noise)
        );
        sample.timestamp_ns = base_timestamp + i * sample_interval;
        
        samples.push_back(sample);
    }
    
    return samples;
}

std::vector<DELILA::WaveformSample> TestDataGenerator::generateNoiseWaveform(
    uint32_t num_samples,
    uint16_t base_value,
    uint16_t noise_amplitude,
    uint64_t base_timestamp,
    uint64_t sample_interval) {
    
    std::vector<DELILA::WaveformSample> samples;
    samples.reserve(num_samples);
    
    for (uint32_t i = 0; i < num_samples; i++) {
        DELILA::WaveformSample sample;
        
        // Generate random noise around base value
        int32_t noise = static_cast<int32_t>(int_dist_(random_engine_) % (2 * noise_amplitude)) - noise_amplitude;
        sample.adc_value = static_cast<uint16_t>(
            std::max(0, std::min(65535, static_cast<int32_t>(base_value) + noise))
        );
        sample.timestamp_ns = base_timestamp + i * sample_interval;
        
        samples.push_back(sample);
    }
    
    return samples;
}

void TestDataGenerator::setSeed(uint32_t seed) {
    current_seed_ = seed == 0 ? std::random_device{}() : seed;
    random_engine_.seed(current_seed_);
}

uint64_t TestDataGenerator::generateTimestamp() {
    return getCurrentTimestampNs() + static_cast<uint64_t>(real_dist_(random_engine_) * 1000000); // +/- 1ms variation
}

size_t TestDataGenerator::calculateEventSize(const DELILA::EventData& event) const {
    return 34 + event.waveform.size() * sizeof(DELILA::WaveformSample); // Header + waveform
}

} // namespace DELILA::Net::Test