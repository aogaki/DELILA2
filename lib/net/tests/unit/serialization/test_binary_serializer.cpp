/**
 * @file test_binary_serializer.cpp
 * @brief Unit tests for BinarySerializer class
 * 
 * Following TDD approach - these tests define the expected behavior
 * for EventData serialization and deserialization.
 */

#include <gtest/gtest.h>
#include <random>
#include "delila/net/serialization/BinarySerializer.hpp"
#include "delila/net/mock/EventData.hpp"
#include "delila/net/utils/Platform.hpp"

using namespace DELILA::Net;
using namespace DELILA;

class BinarySerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer_ = std::make_unique<BinarySerializer>();
        
        // Initialize random number generator for test data
        random_engine_.seed(42); // Fixed seed for reproducible tests
    }
    
    void TearDown() override {
        serializer_.reset();
    }
    
    /**
     * @brief Generate a single test EventData with specified waveform size
     */
    EventData generateSingleEvent(uint32_t waveform_size) {
        EventData event;
        event.analogProbe1Type = uniform_int_dist_(random_engine_) % 256;
        event.analogProbe2Type = uniform_int_dist_(random_engine_) % 256;
        event.channel = uniform_int_dist_(random_engine_) % 64;
        event.digitalProbe1Type = uniform_int_dist_(random_engine_) % 256;
        event.digitalProbe2Type = uniform_int_dist_(random_engine_) % 256;
        event.digitalProbe3Type = uniform_int_dist_(random_engine_) % 256;
        event.digitalProbe4Type = uniform_int_dist_(random_engine_) % 256;
        event.downSampleFactor = 1 + (uniform_int_dist_(random_engine_) % 8);
        event.energy = uniform_int_dist_(random_engine_) % 65536;
        event.energyShort = uniform_int_dist_(random_engine_) % 65536;
        event.flags = uniform_int_dist_(random_engine_);
        event.module = uniform_int_dist_(random_engine_) % 16;
        event.timeResolution = uniform_int_dist_(random_engine_) % 256;
        event.timeStampNs = uniform_real_dist_(random_engine_) * 1e12; // Large timestamp
        event.waveformSize = waveform_size;
        
        // Generate waveform samples
        event.waveform.clear();
        event.waveform.reserve(waveform_size);
        for (uint32_t i = 0; i < waveform_size; i++) {
            WaveformSample sample;
            sample.adc_value = uniform_int_dist_(random_engine_) % 65536;
            sample.timestamp_ns = static_cast<uint64_t>(event.timeStampNs) + i * 10; // 10ns intervals
            event.waveform.push_back(sample);
        }
        
        return event;
    }
    
    /**
     * @brief Generate a batch of test EventData with varying waveform sizes
     */
    std::vector<EventData> generateEventBatch(size_t count, 
                                              uint32_t min_waveform_size, 
                                              uint32_t max_waveform_size);
    
    std::unique_ptr<BinarySerializer> serializer_;
    std::mt19937 random_engine_;
    std::uniform_int_distribution<uint32_t> uniform_int_dist_;
    std::uniform_real_distribution<double> uniform_real_dist_;
};

/**
 * @brief Test BinarySerializer construction and destruction
 */
TEST_F(BinarySerializerTest, ConstructorDestructor) {
    // Construction should succeed without errors
    auto serializer = std::make_unique<BinarySerializer>();
    EXPECT_NE(serializer, nullptr);
    
    // Destruction should be clean (no exceptions)
    serializer.reset();
    EXPECT_EQ(serializer, nullptr);
}

/**
 * @brief Test compression configuration
 */
TEST_F(BinarySerializerTest, CompressionConfiguration) {
    // Test enabling/disabling compression
    serializer_->enableCompression(true);
    serializer_->enableCompression(false);
    
    // Test setting compression levels
    for (int level = 1; level <= 12; level++) {
        serializer_->setCompressionLevel(level);
    }
    
    // Should not crash or throw exceptions
    EXPECT_TRUE(true);
}

/**
 * @brief Test calculateEventSize for empty event
 */
TEST_F(BinarySerializerTest, CalculateEventSizeEmpty) {
    auto event = generateSingleEvent(0); // No waveform
    size_t calculated_size = serializer_->calculateEventSize(event);
    
    // Size should be fixed header size (SerializedEventDataHeader = 34 bytes)
    // All the fields in alphabetical order from SerializedEventDataHeader
    size_t expected_size = 34; // As defined in SerializedEventDataHeader
    EXPECT_EQ(calculated_size, expected_size);
}

/**
 * @brief Test calculateEventSize with waveform data
 */
TEST_F(BinarySerializerTest, CalculateEventSizeWithWaveform) {
    constexpr uint32_t WAVEFORM_SIZE = 1000;
    auto event = generateSingleEvent(WAVEFORM_SIZE);
    size_t calculated_size = serializer_->calculateEventSize(event);
    
    // Size should be header + waveform
    // Header: 28 bytes, Waveform: 1000 * sizeof(WaveformSample) bytes
    size_t expected_size = 34 + (WAVEFORM_SIZE * sizeof(WaveformSample));
    EXPECT_EQ(calculated_size, expected_size);
    // Verify it matches our structure sizes
    EXPECT_EQ(sizeof(WaveformSample), 10); // 2 + 8 bytes packed
}

/**
 * @brief Test single event serialization (empty event)
 */
TEST_F(BinarySerializerTest, SerializeEmptyEvent) {
    auto event = generateSingleEvent(0); // No waveform
    auto result = serializer_->encodeEvent(event);
    
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    EXPECT_EQ(data.size(), 34); // Header size only
    
    // Verify data is not empty
    EXPECT_FALSE(data.empty());
}

/**
 * @brief Test single event serialization with waveform
 */
TEST_F(BinarySerializerTest, SerializeEventWithWaveform) {
    constexpr uint32_t WAVEFORM_SIZE = 500;
    auto event = generateSingleEvent(WAVEFORM_SIZE);
    auto result = serializer_->encodeEvent(event);
    
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    size_t expected_size = 34 + (WAVEFORM_SIZE * sizeof(WaveformSample)); // Header + waveform
    EXPECT_EQ(data.size(), expected_size);
    EXPECT_EQ(data.size(), 34 + (WAVEFORM_SIZE * 10)); // Header + waveform data
}

/**
 * @brief Test single event deserialization
 */
TEST_F(BinarySerializerTest, DeserializeSingleEvent) {
    // First serialize an event
    auto original = generateSingleEvent(100);
    auto encoded = serializer_->encodeEvent(original);
    ASSERT_TRUE(isOk(encoded));
    
    // Then deserialize it
    const auto& data = getValue(encoded);
    auto decoded = serializer_->decodeEvent(data.data(), data.size());
    ASSERT_TRUE(isOk(decoded));
    
    // Verify basic structure (detailed field validation in round-trip test)
    const auto& decoded_event = getValue(decoded);
    EXPECT_EQ(decoded_event.waveformSize, original.waveformSize);
}

/**
 * @brief Test single event round-trip consistency
 */
TEST_F(BinarySerializerTest, SingleEventRoundTrip) {
    // Test with various waveform sizes
    for (uint32_t waveform_size : {0, 1, 10, 100, 1000}) {
        auto original = generateSingleEvent(waveform_size);
        
        // Encode
        auto encoded = serializer_->encodeEvent(original);
        ASSERT_TRUE(isOk(encoded)) << "Failed to encode event with waveform size " << waveform_size;
        
        // Decode
        const auto& data = getValue(encoded);
        auto decoded = serializer_->decodeEvent(data.data(), data.size());
        ASSERT_TRUE(isOk(decoded)) << "Failed to decode event with waveform size " << waveform_size;
        
        // Verify all fields match exactly
        const auto& decoded_event = getValue(decoded);
        EXPECT_EQ(original.analogProbe1Type, decoded_event.analogProbe1Type);
        EXPECT_EQ(original.channel, decoded_event.channel);
        EXPECT_EQ(original.energy, decoded_event.energy);
        EXPECT_EQ(original.energyShort, decoded_event.energyShort);
        EXPECT_DOUBLE_EQ(original.timeStampNs, decoded_event.timeStampNs);
        EXPECT_EQ(original.waveformSize, decoded_event.waveformSize);
        
        // Verify waveform data
        ASSERT_EQ(original.waveform.size(), decoded_event.waveform.size());
        for (size_t i = 0; i < original.waveform.size(); i++) {
            EXPECT_EQ(original.waveform[i].adc_value, decoded_event.waveform[i].adc_value);
            EXPECT_EQ(original.waveform[i].timestamp_ns, decoded_event.waveform[i].timestamp_ns);
        }
    }
}

/**
 * @brief Test serialization with invalid data
 */
TEST_F(BinarySerializerTest, SerializationErrorHandling) {
    // Test with mismatched waveform size and actual waveform data
    auto event = generateSingleEvent(100);
    event.waveformSize = 200; // Mismatch: claim 200 but only have 100 samples
    
    auto result = serializer_->encodeEvent(event);
    
    // Should handle gracefully (either succeed with actual size or return error)
    // Implementation detail: we'll allow this but use actual waveform.size()
    if (isOk(result)) {
        const auto& data = getValue(result);
        // Should use actual waveform size, not the claimed size
        size_t expected_size = 28 + (100 * 12); // 100 samples, not 200
        EXPECT_LE(data.size(), expected_size + 100); // Allow some tolerance
    }
}

/**
 * @brief Test deserialization with invalid data
 */
TEST_F(BinarySerializerTest, DeserializationErrorHandling) {
    // Test with empty buffer
    auto result1 = serializer_->decodeEvent(nullptr, 0);
    EXPECT_FALSE(isOk(result1));
    
    // Test with too small buffer
    uint8_t small_buffer[10] = {0};
    auto result2 = serializer_->decodeEvent(small_buffer, 10);
    EXPECT_FALSE(isOk(result2));
    
    // Test with corrupted header
    auto original = generateSingleEvent(10);
    auto encoded = serializer_->encodeEvent(original);
    ASSERT_TRUE(isOk(encoded));
    
    auto data = getValue(encoded);
    data[0] = 0xFF; // Corrupt first byte
    auto result3 = serializer_->decodeEvent(data.data(), data.size());
    // May succeed or fail depending on what first byte represents
    // The important thing is it doesn't crash
}

/**
 * @brief Test field ordering in serialized data
 */
TEST_F(BinarySerializerTest, FieldOrderingInSerialization) {
    auto event = generateSingleEvent(0); // No waveform for simpler testing
    auto result = serializer_->encodeEvent(event);
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    ASSERT_GE(data.size(), 34);
    
    // Verify field order by parsing manually
    // Fields should be in alphabetical order (matching SerializedEventDataHeader):
    // analogProbe1Type, analogProbe2Type, channel, digitalProbe1Type, digitalProbe2Type,
    // digitalProbe3Type, digitalProbe4Type, downSampleFactor, energy, energyShort,
    // flags, module, timeResolution, timeStampNs, waveformSize
    
    const uint8_t* ptr = data.data();
    
    // analogProbe1Type (1 byte)
    uint8_t analogProbe1Type = *ptr;
    EXPECT_EQ(analogProbe1Type, event.analogProbe1Type);
    ptr += 1;
    
    // analogProbe2Type (1 byte)
    uint8_t analogProbe2Type = *ptr;
    EXPECT_EQ(analogProbe2Type, event.analogProbe2Type);
    ptr += 1;
    
    // channel (1 byte)
    uint8_t channel = *ptr;
    EXPECT_EQ(channel, event.channel);
    ptr += 1;
    
    // digitalProbe1Type (1 byte)
    uint8_t digitalProbe1Type = *ptr;
    EXPECT_EQ(digitalProbe1Type, event.digitalProbe1Type);
    ptr += 1;
    
    // Skip to a few key fields for brevity...
    
    // energy (2 bytes, at offset 8)
    ptr = data.data() + 8;
    uint16_t energy = *reinterpret_cast<const uint16_t*>(ptr);
    EXPECT_EQ(energy, event.energy);
    
    // energyShort (2 bytes, at offset 10)
    ptr = data.data() + 10;
    uint16_t energyShort = *reinterpret_cast<const uint16_t*>(ptr);
    EXPECT_EQ(energyShort, event.energyShort);
    
    // timeStampNs (8 bytes, at offset 22)
    ptr = data.data() + 22;
    double timeStampNs = *reinterpret_cast<const double*>(ptr);
    EXPECT_DOUBLE_EQ(timeStampNs, event.timeStampNs);
    
    // waveformSize (4 bytes, at offset 30)
    ptr = data.data() + 30;
    uint32_t waveformSize = *reinterpret_cast<const uint32_t*>(ptr);
    EXPECT_EQ(waveformSize, event.waveformSize);
}

//=============================================================================
// Batch Serialization Tests (Phase 3.2)
//=============================================================================

/**
 * @brief Generate a batch of test EventData with varying waveform sizes
 */
std::vector<EventData> BinarySerializerTest::generateEventBatch(size_t count, 
                                                                uint32_t min_waveform_size, 
                                                                uint32_t max_waveform_size) {
    std::vector<EventData> events;
    events.reserve(count);
    
    for (size_t i = 0; i < count; i++) {
        // Vary waveform size within range
        uint32_t waveform_size = min_waveform_size;
        if (max_waveform_size > min_waveform_size) {
            waveform_size = min_waveform_size + 
                           (uniform_int_dist_(random_engine_) % (max_waveform_size - min_waveform_size + 1));
        }
        
        events.push_back(generateSingleEvent(waveform_size));
        
        // Make events identifiable by setting module number
        events.back().module = static_cast<uint8_t>(i % 256);
    }
    
    return events;
}

/**
 * @brief Test batch serialization with small batch
 */
TEST_F(BinarySerializerTest, BatchSerializationSmall) {
    auto events = generateEventBatch(5, 10, 100);
    auto result = serializer_->encode(events);
    
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    
    // Should have header + payload
    EXPECT_GE(data.size(), sizeof(BinaryDataHeader));
    
    // Validate header
    const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
    EXPECT_EQ(header->magic_number, MAGIC_NUMBER);
    EXPECT_EQ(header->format_version, CURRENT_FORMAT_VERSION);
    EXPECT_EQ(header->header_size, sizeof(BinaryDataHeader));
    EXPECT_EQ(header->event_count, 5);
    EXPECT_GE(header->sequence_number, 0); // Should be assigned (starts at 0)
    EXPECT_GT(header->timestamp, 0); // Should have timestamp
    EXPECT_EQ(header->compressed_size, header->uncompressed_size); // No compression by default
}

/**
 * @brief Test batch serialization with medium batch
 */
TEST_F(BinarySerializerTest, BatchSerializationMedium) {
    auto events = generateEventBatch(100, 50, 200);
    auto result = serializer_->encode(events);
    
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
    
    EXPECT_EQ(header->event_count, 100);
    EXPECT_GT(header->uncompressed_size, 0);
    
    // Verify payload size matches header
    size_t expected_total_size = sizeof(BinaryDataHeader) + header->uncompressed_size;
    EXPECT_EQ(data.size(), expected_total_size);
}

/**
 * @brief Test batch deserialization
 */
TEST_F(BinarySerializerTest, BatchDeserialization) {
    auto original = generateEventBatch(20, 10, 50);
    
    // Encode
    auto encoded = serializer_->encode(original);
    ASSERT_TRUE(isOk(encoded));
    
    // Decode
    auto decoded = serializer_->decode(getValue(encoded));
    ASSERT_TRUE(isOk(decoded));
    
    const auto& decoded_events = getValue(decoded);
    
    // Verify event count matches
    ASSERT_EQ(decoded_events.size(), original.size());
    
    // Verify first and last events match (spot check)
    EXPECT_EQ(original[0].analogProbe1Type, decoded_events[0].analogProbe1Type);
    EXPECT_EQ(original[0].channel, decoded_events[0].channel);
    EXPECT_EQ(original.back().energy, decoded_events.back().energy);
    EXPECT_EQ(original.back().waveformSize, decoded_events.back().waveformSize);
}

/**
 * @brief Test batch round-trip consistency
 */
TEST_F(BinarySerializerTest, BatchRoundTrip) {
    // Test with various batch sizes
    for (size_t batch_size : {1, 5, 10, 50}) {
        auto original = generateEventBatch(batch_size, 0, 100);
        
        // Encode
        auto encoded = serializer_->encode(original);
        ASSERT_TRUE(isOk(encoded)) << "Failed to encode batch of size " << batch_size;
        
        // Decode
        auto decoded = serializer_->decode(getValue(encoded));
        ASSERT_TRUE(isOk(decoded)) << "Failed to decode batch of size " << batch_size;
        
        const auto& decoded_events = getValue(decoded);
        ASSERT_EQ(decoded_events.size(), original.size()) << "Size mismatch for batch size " << batch_size;
        
        // Verify all events match exactly
        for (size_t i = 0; i < original.size(); i++) {
            EXPECT_EQ(original[i].analogProbe1Type, decoded_events[i].analogProbe1Type) << "Event " << i;
            EXPECT_EQ(original[i].channel, decoded_events[i].channel) << "Event " << i;
            EXPECT_EQ(original[i].energy, decoded_events[i].energy) << "Event " << i;
            EXPECT_EQ(original[i].energyShort, decoded_events[i].energyShort) << "Event " << i;
            EXPECT_DOUBLE_EQ(original[i].timeStampNs, decoded_events[i].timeStampNs) << "Event " << i;
            EXPECT_EQ(original[i].waveformSize, decoded_events[i].waveformSize) << "Event " << i;
            
            // Verify waveform data
            ASSERT_EQ(original[i].waveform.size(), decoded_events[i].waveform.size()) << "Event " << i;
            for (size_t j = 0; j < original[i].waveform.size(); j++) {
                EXPECT_EQ(original[i].waveform[j].adc_value, decoded_events[i].waveform[j].adc_value) 
                    << "Event " << i << ", sample " << j;
                EXPECT_EQ(original[i].waveform[j].timestamp_ns, decoded_events[i].waveform[j].timestamp_ns) 
                    << "Event " << i << ", sample " << j;
            }
        }
    }
}

/**
 * @brief Test sequence number incrementing
 */
TEST_F(BinarySerializerTest, SequenceNumberIncrement) {
    auto events = generateEventBatch(1, 10, 10);
    
    // Encode multiple batches
    auto result1 = serializer_->encode(events);
    auto result2 = serializer_->encode(events);
    auto result3 = serializer_->encode(events);
    
    ASSERT_TRUE(isOk(result1));
    ASSERT_TRUE(isOk(result2));
    ASSERT_TRUE(isOk(result3));
    
    // Extract headers
    const auto* header1 = reinterpret_cast<const BinaryDataHeader*>(getValue(result1).data());
    const auto* header2 = reinterpret_cast<const BinaryDataHeader*>(getValue(result2).data());
    const auto* header3 = reinterpret_cast<const BinaryDataHeader*>(getValue(result3).data());
    
    // Verify sequence numbers increment
    EXPECT_EQ(header2->sequence_number, header1->sequence_number + 1);
    EXPECT_EQ(header3->sequence_number, header2->sequence_number + 1);
}

/**
 * @brief Test empty batch error handling
 */
TEST_F(BinarySerializerTest, EmptyBatchError) {
    std::vector<EventData> empty_events;
    auto result = serializer_->encode(empty_events);
    
    EXPECT_FALSE(isOk(result));
    EXPECT_EQ(getError(result).code, Error::INVALID_DATA);
}

/**
 * @brief Test checksum validation
 */
TEST_F(BinarySerializerTest, ChecksumValidation) {
    auto events = generateEventBatch(5, 10, 50);
    auto encoded = serializer_->encode(events);
    ASSERT_TRUE(isOk(encoded));
    
    auto data = getValue(encoded);
    
    // Corrupt one byte in payload (after header)
    if (data.size() > sizeof(BinaryDataHeader) + 10) {
        data[sizeof(BinaryDataHeader) + 10] ^= 0xFF;
        
        auto decoded = serializer_->decode(data);
        ASSERT_FALSE(isOk(decoded));
        EXPECT_EQ(getError(decoded).code, Error::CHECKSUM_MISMATCH);
    }
}

/**
 * @brief Test invalid header detection
 */
TEST_F(BinarySerializerTest, InvalidHeaderDetection) {
    auto events = generateEventBatch(2, 5, 10);
    auto encoded = serializer_->encode(events);
    ASSERT_TRUE(isOk(encoded));
    
    auto data = getValue(encoded);
    
    // Corrupt magic number
    auto corrupted_data = data;
    corrupted_data[0] = 0xFF;
    
    auto result = serializer_->decode(corrupted_data);
    EXPECT_FALSE(isOk(result));
    EXPECT_EQ(getError(result).code, Error::INVALID_FORMAT);
}

/**
 * @brief Test payload size validation
 */
TEST_F(BinarySerializerTest, PayloadSizeValidation) {
    auto events = generateEventBatch(3, 20, 20);
    auto encoded = serializer_->encode(events);
    ASSERT_TRUE(isOk(encoded));
    
    auto data = getValue(encoded);
    
    // Truncate data (remove some bytes from end)
    if (data.size() > 100) {
        data.resize(data.size() - 50);
        
        auto result = serializer_->decode(data);
        EXPECT_FALSE(isOk(result));
        EXPECT_EQ(getError(result).code, Error::INVALID_DATA);
    }
}

//=============================================================================
// Compression Integration Tests (Phase 4)
//=============================================================================

/**
 * @brief Test LZ4 compression with large data
 */
TEST_F(BinarySerializerTest, CompressionEnabled) {
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(1);
    
    // Generate large events that should benefit from compression
    auto events = generateEventBatch(50, 1000, 1000); // Large waveforms
    auto result = serializer_->encode(events);
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
    
    // Should have compression applied (compressed_size < uncompressed_size)
    EXPECT_LT(header->compressed_size, header->uncompressed_size);
    EXPECT_GT(header->uncompressed_size, 0);
    
    // Total message size should be header + compressed_size
    EXPECT_EQ(data.size(), sizeof(BinaryDataHeader) + header->compressed_size);
}

/**
 * @brief Test compression disabled
 */
TEST_F(BinarySerializerTest, CompressionDisabled) {
    serializer_->enableCompression(false);
    
    auto events = generateEventBatch(10, 100, 100);
    auto result = serializer_->encode(events);
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
    
    // Should have no compression (compressed_size == uncompressed_size)
    EXPECT_EQ(header->compressed_size, header->uncompressed_size);
}

/**
 * @brief Test compression levels
 */
TEST_F(BinarySerializerTest, CompressionLevels) {
    serializer_->enableCompression(true);
    
    auto events = generateEventBatch(20, 500, 500); // Consistent data for comparison
    
    std::vector<size_t> compressed_sizes;
    
    // Test different compression levels
    for (int level = 1; level <= 3; level++) {
        serializer_->setCompressionLevel(level);
        auto result = serializer_->encode(events);
        ASSERT_TRUE(isOk(result)) << "Failed at compression level " << level;
        
        const auto& data = getValue(result);
        const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
        compressed_sizes.push_back(header->compressed_size);
    }
    
    // All should be valid compressed sizes
    for (size_t size : compressed_sizes) {
        EXPECT_GT(size, 0);
    }
}

/**
 * @brief Test compression round-trip consistency
 */
TEST_F(BinarySerializerTest, CompressionRoundTrip) {
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(1);
    
    auto original = generateEventBatch(15, 200, 800);
    
    // Encode with compression
    auto encoded = serializer_->encode(original);
    ASSERT_TRUE(isOk(encoded));
    
    // Decode
    auto decoded = serializer_->decode(getValue(encoded));
    ASSERT_TRUE(isOk(decoded));
    
    const auto& decoded_events = getValue(decoded);
    ASSERT_EQ(decoded_events.size(), original.size());
    
    // Verify first and last events match exactly
    EXPECT_EQ(original[0].energy, decoded_events[0].energy);
    EXPECT_EQ(original[0].energyShort, decoded_events[0].energyShort);
    EXPECT_EQ(original.back().waveformSize, decoded_events.back().waveformSize);
    
    // Verify waveform data integrity
    if (!original[0].waveform.empty()) {
        EXPECT_EQ(original[0].waveform[0].adc_value, decoded_events[0].waveform[0].adc_value);
    }
}

/**
 * @brief Test compression with small data (should not compress)
 */
TEST_F(BinarySerializerTest, CompressionSmallData) {
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(1);
    
    // Small data that might not benefit from compression
    auto events = generateEventBatch(2, 5, 10);
    auto result = serializer_->encode(events);
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
    
    // Should still work (might not actually compress small data)
    EXPECT_GT(header->uncompressed_size, 0);
    EXPECT_GT(header->compressed_size, 0);
}

/**
 * @brief Test checksum validation with correct data
 */
TEST_F(BinarySerializerTest, ChecksumValidationCorrect) {
    auto events = generateEventBatch(10, 50, 100);
    auto encoded = serializer_->encode(events);
    ASSERT_TRUE(isOk(encoded));
    
    // Should decode successfully (checksum matches)
    auto decoded = serializer_->decode(getValue(encoded));
    ASSERT_TRUE(isOk(decoded));
    
    const auto& decoded_events = getValue(decoded);
    EXPECT_EQ(decoded_events.size(), events.size());
}

/**
 * @brief Test checksum validation with corrupted data
 */
TEST_F(BinarySerializerTest, ChecksumValidationCorrupted) {
    auto events = generateEventBatch(5, 50, 100);
    auto encoded_result = serializer_->encode(events);
    ASSERT_TRUE(isOk(encoded_result));
    
    auto data = getValue(encoded_result);
    
    // Corrupt a byte in the payload (after header)
    if (data.size() > sizeof(BinaryDataHeader) + 10) {
        data[sizeof(BinaryDataHeader) + 5] ^= 0xFF;
        
        auto decoded = serializer_->decode(data);
        ASSERT_FALSE(isOk(decoded));
        EXPECT_EQ(getError(decoded).code, Error::CHECKSUM_MISMATCH);
    }
}

/**
 * @brief Test compression with various data patterns
 */
TEST_F(BinarySerializerTest, CompressionDataPatterns) {
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(1);
    
    // Test with repetitive data (should compress well)
    auto events = generateEventBatch(30, 100, 100); // Same waveform size
    
    // Set similar values to create patterns
    for (auto& event : events) {
        event.energy = 1000;  // Same energy
        event.energyShort = 500;  // Same short energy
        // Waveform will have similar patterns from the generator
    }
    
    auto result = serializer_->encode(events);
    ASSERT_TRUE(isOk(result));
    
    const auto& data = getValue(result);
    const auto* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
    
    // Should achieve some compression ratio (or at least not expand)
    double compression_ratio = static_cast<double>(header->compressed_size) / header->uncompressed_size;
    EXPECT_LE(compression_ratio, 1.0); // Should not expand the data
    
    // If compression was applied, it should be beneficial
    if (header->compressed_size < header->uncompressed_size) {
        EXPECT_LT(compression_ratio, 0.95); // At least 5% compression
    }
}