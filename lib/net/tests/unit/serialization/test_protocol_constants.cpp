/**
 * @file test_protocol_constants.cpp
 * @brief Unit tests for Protocol Constants and Header Structures
 * 
 * Following TDD approach - these tests define the expected behavior
 * before implementation.
 */

#include <gtest/gtest.h>
#include <cstring>
#include "delila/net/serialization/ProtocolConstants.hpp"

using namespace DELILA::Net;

class ProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup if needed
    }
    
    void TearDown() override {
        // Test cleanup if needed  
    }
};

/**
 * @brief Test magic number constant is correctly defined
 */
TEST_F(ProtocolTest, MagicNumberConstant) {
    // Magic number should be "DELILA2\0" as 64-bit little-endian value
    EXPECT_EQ(MAGIC_NUMBER, 0x44454C494C413200ULL);
    
    // Verify it represents the correct string when interpreted as bytes (little-endian)
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&MAGIC_NUMBER);
    EXPECT_EQ(bytes[0], '\0'); // 0x00 (LSB)
    EXPECT_EQ(bytes[1], '2');  // 0x32
    EXPECT_EQ(bytes[2], 'A');  // 0x41
    EXPECT_EQ(bytes[3], 'L');  // 0x4C
    EXPECT_EQ(bytes[4], 'I');  // 0x49
    EXPECT_EQ(bytes[5], 'L');  // 0x4C
    EXPECT_EQ(bytes[6], 'E');  // 0x45
    EXPECT_EQ(bytes[7], 'D');  // 0x44 (MSB)
}

/**
 * @brief Test protocol version and header size constants
 */
TEST_F(ProtocolTest, ProtocolConstants) {
    EXPECT_EQ(CURRENT_FORMAT_VERSION, 1);
    EXPECT_EQ(HEADER_SIZE, 64);
    
    // Performance limits
    EXPECT_EQ(MAX_MESSAGE_SIZE, 1024 * 1024);        // 1MB
    EXPECT_EQ(MAX_EVENTS_PER_MESSAGE, 10000);
    EXPECT_EQ(MIN_MESSAGE_SIZE, 100 * 1024);         // 100KB
    
    // Compression settings
    EXPECT_EQ(DEFAULT_COMPRESSION_LEVEL, 1);
}

/**
 * @brief Test BinaryDataHeader structure size and alignment
 */
TEST_F(ProtocolTest, BinaryDataHeaderSize) {
    // Header must be exactly 64 bytes
    EXPECT_EQ(sizeof(BinaryDataHeader), 64);
    
    // Verify field offsets (little-endian layout)
    EXPECT_EQ(offsetof(BinaryDataHeader, magic_number), 0);
    EXPECT_EQ(offsetof(BinaryDataHeader, sequence_number), 8);
    EXPECT_EQ(offsetof(BinaryDataHeader, format_version), 16);
    EXPECT_EQ(offsetof(BinaryDataHeader, header_size), 20);
    EXPECT_EQ(offsetof(BinaryDataHeader, event_count), 24);
    EXPECT_EQ(offsetof(BinaryDataHeader, uncompressed_size), 28);
    EXPECT_EQ(offsetof(BinaryDataHeader, compressed_size), 32);
    EXPECT_EQ(offsetof(BinaryDataHeader, checksum), 36);
    EXPECT_EQ(offsetof(BinaryDataHeader, timestamp), 40);
    EXPECT_EQ(offsetof(BinaryDataHeader, reserved), 48);
}

/**
 * @brief Test BinaryDataHeader field initialization and validation
 */
TEST_F(ProtocolTest, BinaryDataHeaderInitialization) {
    BinaryDataHeader header{};
    
    // Initialize header with known values
    header.magic_number = MAGIC_NUMBER;
    header.sequence_number = 42;
    header.format_version = CURRENT_FORMAT_VERSION;
    header.header_size = HEADER_SIZE;
    header.event_count = 100;
    header.uncompressed_size = 5000;
    header.compressed_size = 4000;
    header.checksum = 0x12345678;
    header.timestamp = 1234567890123456789ULL;
    std::memset(header.reserved, 0, sizeof(header.reserved));
    
    // Verify all fields
    EXPECT_EQ(header.magic_number, MAGIC_NUMBER);
    EXPECT_EQ(header.sequence_number, 42);
    EXPECT_EQ(header.format_version, CURRENT_FORMAT_VERSION);
    EXPECT_EQ(header.header_size, HEADER_SIZE);
    EXPECT_EQ(header.event_count, 100);
    EXPECT_EQ(header.uncompressed_size, 5000);
    EXPECT_EQ(header.compressed_size, 4000);
    EXPECT_EQ(header.checksum, 0x12345678);
    EXPECT_EQ(header.timestamp, 1234567890123456789ULL);
}

/**
 * @brief Test SerializedEventDataHeader structure size and alignment
 */
TEST_F(ProtocolTest, SerializedEventDataHeaderSize) {
    // Header must be exactly 34 bytes (packed, no padding)
    EXPECT_EQ(sizeof(SerializedEventDataHeader), 34);
    
    // Verify field offsets (alphabetical order, packed)
    EXPECT_EQ(offsetof(SerializedEventDataHeader, analogProbe1Type), 0);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, analogProbe2Type), 1);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, channel), 2);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, digitalProbe1Type), 3);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, digitalProbe2Type), 4);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, digitalProbe3Type), 5);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, digitalProbe4Type), 6);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, downSampleFactor), 7);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, energy), 8);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, energyShort), 10);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, flags), 12);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, module), 20);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, timeResolution), 21);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, timeStampNs), 22);
    EXPECT_EQ(offsetof(SerializedEventDataHeader, waveformSize), 30);
}

/**
 * @brief Test SerializedEventDataHeader field initialization
 */
TEST_F(ProtocolTest, SerializedEventDataHeaderInitialization) {
    SerializedEventDataHeader event_header{};
    
    // Initialize with test values (alphabetical order)
    event_header.analogProbe1Type = 1;
    event_header.analogProbe2Type = 2;
    event_header.channel = 5;
    event_header.digitalProbe1Type = 3;
    event_header.digitalProbe2Type = 4;
    event_header.digitalProbe3Type = 5;
    event_header.digitalProbe4Type = 6;
    event_header.downSampleFactor = 2;
    event_header.energy = 1000;
    event_header.energyShort = 500;
    event_header.flags = 0x123456789ABCDEF0ULL;
    event_header.module = 7;
    event_header.timeResolution = 8;
    event_header.timeStampNs = 1234567890.123456;
    event_header.waveformSize = 2048;
    
    // Verify all fields
    EXPECT_EQ(event_header.analogProbe1Type, 1);
    EXPECT_EQ(event_header.analogProbe2Type, 2);
    EXPECT_EQ(event_header.channel, 5);
    EXPECT_EQ(event_header.digitalProbe1Type, 3);
    EXPECT_EQ(event_header.digitalProbe2Type, 4);
    EXPECT_EQ(event_header.digitalProbe3Type, 5);
    EXPECT_EQ(event_header.digitalProbe4Type, 6);
    EXPECT_EQ(event_header.downSampleFactor, 2);
    EXPECT_EQ(event_header.energy, 1000);
    EXPECT_EQ(event_header.energyShort, 500);
    EXPECT_EQ(event_header.flags, 0x123456789ABCDEF0ULL);
    EXPECT_EQ(event_header.module, 7);
    EXPECT_EQ(event_header.timeResolution, 8);
    EXPECT_DOUBLE_EQ(event_header.timeStampNs, 1234567890.123456);
    EXPECT_EQ(event_header.waveformSize, 2048);
}

/**
 * @brief Test byte order validation functions
 */  
TEST_F(ProtocolTest, ByteOrderValidation) {
    // Test little-endian validation
    EXPECT_TRUE(isLittleEndian());
    
    // Test multi-byte value byte order
    uint32_t test_value = 0x12345678;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&test_value);
    
    // In little-endian: least significant byte first
    EXPECT_EQ(bytes[0], 0x78);  // LSB
    EXPECT_EQ(bytes[1], 0x56);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);  // MSB
}

/**
 * @brief Test header packing - no padding between fields
 */
TEST_F(ProtocolTest, HeaderPacking) {
    // BinaryDataHeader should have no padding
    size_t expected_binary_size = 8 + 8 + 4 + 4 + 4 + 4 + 4 + 4 + 8 + 16; // 64 bytes
    EXPECT_EQ(sizeof(BinaryDataHeader), expected_binary_size);
    
    // SerializedEventDataHeader should have no padding
    size_t expected_event_size = 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 2 + 2 + 8 + 1 + 1 + 8 + 4; // 34 bytes
    EXPECT_EQ(sizeof(SerializedEventDataHeader), expected_event_size);
}

/**
 * @brief Test waveform size calculation
 */
TEST_F(ProtocolTest, WaveformSizeCalculation) {
    uint32_t waveformSize = 1000;
    
    // Calculate total waveform data size
    // analogProbe1: waveformSize * sizeof(int32_t) = 1000 * 4 = 4000
    // analogProbe2: waveformSize * sizeof(int32_t) = 1000 * 4 = 4000  
    // digitalProbe1-4: waveformSize * sizeof(uint8_t) * 4 = 1000 * 1 * 4 = 4000
    // Total: 4000 + 4000 + 4000 = 12000 bytes
    
    size_t expected_waveform_size = waveformSize * (4 + 4 + 1 + 1 + 1 + 1); // 12 bytes per sample
    EXPECT_EQ(expected_waveform_size, waveformSize * 12);
    
    // Total EventData size = header (34) + waveform data
    size_t total_event_size = 34 + expected_waveform_size;
    EXPECT_EQ(total_event_size, 34 + waveformSize * 12);
}

/**
 * @brief Test compile-time assertions are enforced
 */
TEST_F(ProtocolTest, CompileTimeAssertions) {
    // These should compile successfully due to static_assert in headers
    // If they fail, compilation will fail (which is what we want)
    
    // Verify little-endian platform (this is checked at compile-time)
    EXPECT_TRUE(true); // Placeholder - actual check is static_assert in header
    
    // Verify header sizes are as expected (checked at compile-time)
    static_assert(sizeof(BinaryDataHeader) == 64, "BinaryDataHeader must be 64 bytes");
    static_assert(sizeof(SerializedEventDataHeader) == 34, "SerializedEventDataHeader must be 34 bytes");
}