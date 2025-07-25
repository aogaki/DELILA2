#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @file ProtocolConstants.hpp
 * @brief Protocol constants and header structures for DELILA2 binary serialization
 * 
 * Defines all constants, magic numbers, and binary header structures used
 * in the DELILA2 networking protocol.
 */

namespace DELILA::Net {

// Compile-time endianness check - MUST be little-endian
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, 
              "DELILA networking library requires little-endian platform");

//=============================================================================
// Protocol Constants
//=============================================================================

/**
 * @brief Magic number identifier for DELILA2 binary format
 * 
 * Represents "DELILA2\0" as a 64-bit little-endian value.
 * Used for format validation and corruption detection.
 */
constexpr uint64_t MAGIC_NUMBER = 0x44454C494C413200ULL; // "DELILA2\0"

/**
 * @brief Current binary format version
 * 
 * Incremented when binary format changes in incompatible ways.
 * Version 1 is the initial implementation.
 */
constexpr uint32_t CURRENT_FORMAT_VERSION = 1;

/**
 * @brief Fixed size of binary data header in bytes
 * 
 * Header is always 64 bytes for alignment and future expansion.
 */
constexpr uint32_t HEADER_SIZE = 64;

//=============================================================================
// Performance Limits
//=============================================================================

/**
 * @brief Maximum message size limit (1MB)
 * 
 * Prevents excessive memory allocation and network timeouts.
 */
constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024;

/**
 * @brief Maximum events per message for batching
 * 
 * Configurable limit to balance throughput vs latency.
 */
constexpr size_t MAX_EVENTS_PER_MESSAGE = 10000;

/**
 * @brief Minimum message size for compression efficiency
 * 
 * Messages smaller than this may not benefit from compression.
 */
constexpr size_t MIN_MESSAGE_SIZE = 100 * 1024;

//=============================================================================
// Compression Settings
//=============================================================================

/**
 * @brief Default LZ4 compression level (fast compression)
 * 
 * Level 1 provides good speed/compression ratio balance.
 */
constexpr int DEFAULT_COMPRESSION_LEVEL = 1;

//=============================================================================
// Binary Header Structures
//=============================================================================

/**
 * @brief Binary data header structure (uncompressed, fixed 64 bytes)
 * 
 * All multi-byte integers use little-endian byte order.
 * This header precedes all binary data payloads.
 */
struct __attribute__((packed)) BinaryDataHeader {
    uint64_t magic_number;      ///< Magic number: 0x44454C494C413200 ("DELILA2\0")
    uint64_t sequence_number;   ///< Sequence number for gap detection (starts at 0)
    uint32_t format_version;    ///< Binary format version (starts at 1)
    uint32_t header_size;       ///< Size of this header (always 64 bytes)
    uint32_t event_count;       ///< Number of events in payload
    uint32_t uncompressed_size; ///< Size before compression
    uint32_t compressed_size;   ///< Size after compression (equals uncompressed_size if no compression)
    uint32_t checksum;          ///< xxHash32 of payload data
    uint64_t timestamp;         ///< Unix timestamp in nanoseconds since epoch
    uint32_t reserved[4];       ///< Reserved for future expansion (16 bytes)
}; // Total: 64 bytes

// Compile-time size verification
static_assert(sizeof(BinaryDataHeader) == 64, "BinaryDataHeader must be exactly 64 bytes");

/**
 * @brief Serialized EventData header structure (packed, 44 bytes)
 * 
 * Based on DELILA::Digitizer::EventData from EventData.hpp.
 * Fields are serialized in ALPHABETICAL ORDER for consistency.
 * All multi-byte integers use little-endian byte order.
 */
struct __attribute__((packed)) SerializedEventDataHeader {
    uint8_t analogProbe1Type;   ///< Analog probe 1 type
    uint8_t analogProbe2Type;   ///< Analog probe 2 type  
    uint8_t channel;            ///< Channel number
    uint8_t digitalProbe1Type;  ///< Digital probe 1 type
    uint8_t digitalProbe2Type;  ///< Digital probe 2 type
    uint8_t digitalProbe3Type;  ///< Digital probe 3 type
    uint8_t digitalProbe4Type;  ///< Digital probe 4 type
    uint8_t downSampleFactor;   ///< Down-sampling factor
    uint16_t energy;            ///< Energy value
    uint16_t energyShort;       ///< Short energy value
    uint64_t flags;             ///< Status flags (pileup, trigger lost, etc.)
    uint8_t module;             ///< Module number
    uint8_t timeResolution;     ///< Time resolution setting
    double timeStampNs;         ///< Timestamp in nanoseconds
    uint32_t waveformSize;      ///< Waveform size (FIXED SIZE, not size_t)
}; // Total: 34 bytes

// Compile-time size verification
static_assert(sizeof(SerializedEventDataHeader) == 34, "SerializedEventDataHeader must be exactly 34 bytes");

//=============================================================================
// Waveform Data Layout (follows SerializedEventDataHeader)
//=============================================================================

/**
 * @brief Calculate total waveform data size in bytes
 * 
 * Waveform vectors are serialized in alphabetical order:
 * 1. analogProbe1: waveformSize * sizeof(int32_t) bytes
 * 2. analogProbe2: waveformSize * sizeof(int32_t) bytes  
 * 3. digitalProbe1: waveformSize * sizeof(uint8_t) bytes
 * 4. digitalProbe2: waveformSize * sizeof(uint8_t) bytes
 * 5. digitalProbe3: waveformSize * sizeof(uint8_t) bytes
 * 6. digitalProbe4: waveformSize * sizeof(uint8_t) bytes
 * 
 * @param waveformSize Number of samples in each waveform vector
 * @return Total waveform data size in bytes
 */
constexpr size_t calculateWaveformDataSize(uint32_t waveformSize) {
    return waveformSize * (sizeof(int32_t) + sizeof(int32_t) + 
                          sizeof(uint8_t) + sizeof(uint8_t) + 
                          sizeof(uint8_t) + sizeof(uint8_t));
    // = waveformSize * (4 + 4 + 1 + 1 + 1 + 1) = waveformSize * 12 bytes
}

/**
 * @brief Calculate total EventData serialized size
 * 
 * @param waveformSize Number of samples in waveform vectors
 * @return Total serialized size (header + waveform data)
 */
constexpr size_t calculateEventDataSize(uint32_t waveformSize) {
    return sizeof(SerializedEventDataHeader) + calculateWaveformDataSize(waveformSize);
    // = 34 + waveformSize * 12 bytes
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if current platform is little-endian
 * 
 * @return true if little-endian, false if big-endian
 */
inline bool isLittleEndian() {
    constexpr uint32_t test_value = 0x12345678;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&test_value);
    return bytes[0] == 0x78; // Little-endian: least significant byte first
}

} // namespace DELILA::Net