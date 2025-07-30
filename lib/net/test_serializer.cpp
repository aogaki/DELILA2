#include "Serializer.hpp"

#include <gtest/gtest.h>
#include "Serializer.hpp"
#include "EventData.hpp"
#include <memory>
#include <vector>

namespace DELILA::Net {

class SerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer = std::make_unique<Serializer>();
    }

    void TearDown() override {
        serializer.reset();
    }

    std::unique_ptr<Serializer> serializer;
};

TEST_F(SerializerTest, ConstructorCreatesValidInstance) {
    ASSERT_NE(serializer, nullptr);
}

TEST_F(SerializerTest, EncodeDecodeEmptyEventList) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->size(), 0);
    EXPECT_EQ(sequenceNumber, 0);
}

TEST_F(SerializerTest, EncodeDecodeSingleEvent) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    auto event = std::make_unique<EventData>();
    event->timeStampNs = 1234567890.0;
    event->energy = 1000;
    event->module = 1;
    event->channel = 2;
    event->flags = EventData::FLAG_PILEUP;
    
    events->push_back(std::move(event));
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    ASSERT_GT(encoded->size(), 0);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 0);
    
    const auto& decodedEvent = (*decoded)[0];
    EXPECT_DOUBLE_EQ(decodedEvent->timeStampNs, 1234567890.0);
    EXPECT_EQ(decodedEvent->energy, 1000);
    EXPECT_EQ(decodedEvent->module, 1);
    EXPECT_EQ(decodedEvent->channel, 2);
    EXPECT_EQ(decodedEvent->flags, EventData::FLAG_PILEUP);
}

TEST_F(SerializerTest, EncodeDecodeMultipleEvents) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    const size_t numEvents = 10;
    
    for (size_t i = 0; i < numEvents; ++i) {
        auto event = std::make_unique<EventData>();
        event->timeStampNs = 1000.0 * i;
        event->energy = 100 * i;
        event->module = i % 4;
        event->channel = i % 8;
        events->push_back(std::move(event));
    }
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    ASSERT_GT(encoded->size(), 0);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), numEvents);
    EXPECT_EQ(sequenceNumber, 0);
    
    for (size_t i = 0; i < numEvents; ++i) {
        const auto& decodedEvent = (*decoded)[i];
        EXPECT_DOUBLE_EQ(decodedEvent->timeStampNs, 1000.0 * i);
        EXPECT_EQ(decodedEvent->energy, 100 * i);
        EXPECT_EQ(decodedEvent->module, i % 4);
        EXPECT_EQ(decodedEvent->channel, i % 8);
    }
}

TEST_F(SerializerTest, EncodeDecodeWithWaveforms) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    auto event = std::make_unique<EventData>(100);
    event->timeStampNs = 9876543210.0;
    event->energy = 2000;
    
    for (size_t i = 0; i < 100; ++i) {
        event->analogProbe1[i] = i * 10;
        event->analogProbe2[i] = i * 20;
    }
    
    events->push_back(std::move(event));
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 0);
    
    const auto& decodedEvent = (*decoded)[0];
    EXPECT_EQ(decodedEvent->waveformSize, 100);
    EXPECT_EQ(decodedEvent->analogProbe1.size(), 100);
    EXPECT_EQ(decodedEvent->analogProbe2.size(), 100);
    
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(decodedEvent->analogProbe1[i], i * 10);
        EXPECT_EQ(decodedEvent->analogProbe2[i], i * 20);
    }
}

TEST_F(SerializerTest, CompressionCanBeDisabled) {
    serializer->EnableCompression(false);
    
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->energy = 1500;
    events->push_back(std::move(event));
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 0);
    EXPECT_EQ((*decoded)[0]->energy, 1500);
}

TEST_F(SerializerTest, ChecksumCanBeDisabled) {
    serializer->EnableChecksum(false);
    
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->energy = 2500;
    events->push_back(std::move(event));
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 0);
    EXPECT_EQ((*decoded)[0]->energy, 2500);
}

TEST_F(SerializerTest, DataIntegrityWithAllFeatures) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    auto event = std::make_unique<EventData>(50);
    event->timeStampNs = 123456789.123;
    event->energy = 3000;
    event->energyShort = 2800;
    event->module = 3;
    event->channel = 7;
    event->timeResolution = 2;
    event->analogProbe1Type = 1;
    event->analogProbe2Type = 2;
    event->digitalProbe1Type = 3;
    event->digitalProbe2Type = 4;
    event->digitalProbe3Type = 5;
    event->digitalProbe4Type = 6;
    event->downSampleFactor = 8;
    event->flags = EventData::FLAG_PILEUP | EventData::FLAG_OVER_RANGE;
    
    for (size_t i = 0; i < 50; ++i) {
        event->analogProbe1[i] = i;
        event->analogProbe2[i] = i * 2;
        event->digitalProbe1[i] = i % 2;
        event->digitalProbe2[i] = (i + 1) % 2;
        event->digitalProbe3[i] = i % 4;
        event->digitalProbe4[i] = (i + 2) % 4;
    }
    
    events->push_back(std::move(event));
    
    auto encoded = serializer->Encode(events);
    ASSERT_NE(encoded, nullptr);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 0);
    
    const auto& decodedEvent = (*decoded)[0];
    EXPECT_DOUBLE_EQ(decodedEvent->timeStampNs, 123456789.123);
    EXPECT_EQ(decodedEvent->energy, 3000);
    EXPECT_EQ(decodedEvent->energyShort, 2800);
    EXPECT_EQ(decodedEvent->module, 3);
    EXPECT_EQ(decodedEvent->channel, 7);
    EXPECT_EQ(decodedEvent->timeResolution, 2);
    EXPECT_EQ(decodedEvent->analogProbe1Type, 1);
    EXPECT_EQ(decodedEvent->analogProbe2Type, 2);
    EXPECT_EQ(decodedEvent->digitalProbe1Type, 3);
    EXPECT_EQ(decodedEvent->digitalProbe2Type, 4);
    EXPECT_EQ(decodedEvent->digitalProbe3Type, 5);
    EXPECT_EQ(decodedEvent->digitalProbe4Type, 6);
    EXPECT_EQ(decodedEvent->downSampleFactor, 8);
    EXPECT_EQ(decodedEvent->flags, EventData::FLAG_PILEUP | EventData::FLAG_OVER_RANGE);
    EXPECT_TRUE(decodedEvent->HasPileup());
    EXPECT_TRUE(decodedEvent->HasOverRange());
    EXPECT_FALSE(decodedEvent->HasTriggerLost());
}

TEST_F(SerializerTest, SequenceNumberHandling) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->energy = 1000;
    events->push_back(std::move(event));
    
    // Test with specific sequence number
    uint64_t testSequenceNumber = 12345;
    auto encoded = serializer->Encode(events, testSequenceNumber);
    ASSERT_NE(encoded, nullptr);
    
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, testSequenceNumber);
    EXPECT_EQ((*decoded)[0]->energy, 1000);
}

TEST_F(SerializerTest, CompressionActuallyWorks) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    // Create event with highly compressible waveform data (lots of zeros and repeating patterns)
    auto event = std::make_unique<EventData>(1000);
    event->energy = 5000;
    
    // Fill with repeating pattern that should compress well
    for (size_t i = 0; i < 1000; ++i) {
        event->analogProbe1[i] = (i % 10) == 0 ? 100 : 0;  // Mostly zeros with some spikes
        event->analogProbe2[i] = (i % 5) == 0 ? 200 : 0;   // Even more compressible pattern
    }
    
    events->push_back(std::move(event));
    
    // Enable compression
    serializer->EnableCompression(true);
    
    auto encodedCompressed = serializer->Encode(events, 100);
    ASSERT_NE(encodedCompressed, nullptr);
    
    // Disable compression for comparison
    serializer->EnableCompression(false);
    
    auto encodedUncompressed = serializer->Encode(events, 100);
    ASSERT_NE(encodedUncompressed, nullptr);
    
    // Compressed version should be smaller
    EXPECT_LT(encodedCompressed->size(), encodedUncompressed->size());
    
    // Both should decode to the same data
    serializer->EnableCompression(true);
    auto [decodedCompressed, seqNumCompressed] = serializer->Decode(encodedCompressed);
    
    serializer->EnableCompression(false);
    auto [decodedUncompressed, seqNumUncompressed] = serializer->Decode(encodedUncompressed);
    
    ASSERT_NE(decodedCompressed, nullptr);
    ASSERT_NE(decodedUncompressed, nullptr);
    EXPECT_EQ(seqNumCompressed, 100);
    EXPECT_EQ(seqNumUncompressed, 100);
    
    // Data should be identical
    ASSERT_EQ(decodedCompressed->size(), 1);
    ASSERT_EQ(decodedUncompressed->size(), 1);
    
    const auto& eventCompressed = (*decodedCompressed)[0];
    const auto& eventUncompressed = (*decodedUncompressed)[0];
    
    EXPECT_EQ(eventCompressed->energy, eventUncompressed->energy);
    EXPECT_EQ(eventCompressed->analogProbe1.size(), eventUncompressed->analogProbe1.size());
    EXPECT_EQ(eventCompressed->analogProbe2.size(), eventUncompressed->analogProbe2.size());
    
    for (size_t i = 0; i < 1000; ++i) {
        EXPECT_EQ(eventCompressed->analogProbe1[i], eventUncompressed->analogProbe1[i]);
        EXPECT_EQ(eventCompressed->analogProbe2[i], eventUncompressed->analogProbe2[i]);
    }
}

TEST_F(SerializerTest, ChecksumValidation) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->energy = 3000;
    event->timeStampNs = 123456789.0;
    events->push_back(std::move(event));
    
    // Enable checksum
    serializer->EnableChecksum(true);
    
    auto encoded = serializer->Encode(events, 200);
    ASSERT_NE(encoded, nullptr);
    
    // Should decode successfully with valid checksum
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 200);
    EXPECT_EQ((*decoded)[0]->energy, 3000);
    
    // Corrupt the payload data (skip the 64-byte header) to test checksum validation
    if (encoded->size() > 80) {
        (*encoded)[70] ^= 0xFF;  // Flip some bits in the payload area
    }
    
    // Should fail to decode with corrupted data
    auto [decodedCorrupted, seqNumCorrupted] = serializer->Decode(encoded);
    EXPECT_EQ(decodedCorrupted, nullptr);
    EXPECT_EQ(seqNumCorrupted, 0);
}

TEST_F(SerializerTest, ChecksumWithCompressionIntegration) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    
    // Create compressible data
    auto event = std::make_unique<EventData>(500);
    event->energy = 4000;
    
    for (size_t i = 0; i < 500; ++i) {
        event->analogProbe1[i] = (i % 20) == 0 ? 150 : 0;  // Compressible pattern
    }
    
    events->push_back(std::move(event));
    
    // Enable both compression and checksum
    serializer->EnableCompression(true);
    serializer->EnableChecksum(true);
    
    auto encoded = serializer->Encode(events, 300);
    ASSERT_NE(encoded, nullptr);
    
    // Should decode successfully
    auto [decoded, sequenceNumber] = serializer->Decode(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ(sequenceNumber, 300);
    EXPECT_EQ((*decoded)[0]->energy, 4000);
    
    // Verify waveform data integrity
    const auto& decodedEvent = (*decoded)[0];
    EXPECT_EQ(decodedEvent->analogProbe1.size(), 500);
    
    for (size_t i = 0; i < 500; ++i) {
        EXPECT_EQ(decodedEvent->analogProbe1[i], (i % 20) == 0 ? 150 : 0);
    }
}

}  // namespace DELILA::Net

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

