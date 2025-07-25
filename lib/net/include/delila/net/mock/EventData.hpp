/**
 * @file EventData.hpp
 * @brief Mock EventData structure for binary serialization testing
 * 
 * This is a temporary mock implementation to support development
 * of the binary serialization module until the real EventData
 * from the digitizer library is available.
 */

#pragma once

#include <vector>
#include <cstdint>

namespace DELILA {

/**
 * @brief Waveform sample data structure
 */
struct WaveformSample {
    uint16_t adc_value;    ///< ADC value (2 bytes)
    uint64_t timestamp_ns; ///< Timestamp in nanoseconds (8 bytes)
    
    // Total size: 12 bytes per sample (with 2 bytes padding)
} __attribute__((packed));

/**
 * @brief Mock EventData structure matching expected digitizer format
 * 
 * Fields are ordered alphabetically as per serialization requirements.
 * This structure represents a single digitized event from the DAQ system.
 * 
 * Note: This matches the SerializedEventDataHeader structure from ProtocolConstants.hpp
 */
struct EventData {
    // Alphabetically ordered fields for serialization (matching SerializedEventDataHeader)
    uint8_t analogProbe1Type;    ///< Analog probe 1 type (1 byte)
    uint8_t analogProbe2Type;    ///< Analog probe 2 type (1 byte)  
    uint8_t channel;             ///< Channel number (1 byte)
    uint8_t digitalProbe1Type;   ///< Digital probe 1 type (1 byte)
    uint8_t digitalProbe2Type;   ///< Digital probe 2 type (1 byte)
    uint8_t digitalProbe3Type;   ///< Digital probe 3 type (1 byte)
    uint8_t digitalProbe4Type;   ///< Digital probe 4 type (1 byte)
    uint8_t downSampleFactor;    ///< Down-sampling factor (1 byte)
    uint16_t energy;             ///< Energy value (2 bytes)
    uint16_t energyShort;        ///< Short energy value (2 bytes)
    uint64_t flags;              ///< Status flags (8 bytes)
    uint8_t module;              ///< Module number (1 byte)
    uint8_t timeResolution;      ///< Time resolution setting (1 byte)
    double timeStampNs;          ///< Timestamp in nanoseconds (8 bytes)
    uint32_t waveformSize;       ///< Number of waveform samples (4 bytes)
    
    // Variable-length waveform data
    std::vector<WaveformSample> waveform; ///< Waveform samples
    
    /**
     * @brief Calculate total serialized size of this event
     * @return Size in bytes (header + waveform data)
     */
    size_t getSerializedSize() const {
        return sizeof(analogProbe1Type) + sizeof(analogProbe2Type) + sizeof(channel) +
               sizeof(digitalProbe1Type) + sizeof(digitalProbe2Type) + sizeof(digitalProbe3Type) +
               sizeof(digitalProbe4Type) + sizeof(downSampleFactor) + sizeof(energy) +
               sizeof(energyShort) + sizeof(flags) + sizeof(module) + sizeof(timeResolution) +
               sizeof(timeStampNs) + sizeof(waveformSize) + (waveformSize * sizeof(WaveformSample));
        // Should equal 34 + waveformSize * 10 bytes
    }
    
    /**
     * @brief Default constructor
     */
    EventData() : analogProbe1Type(0), analogProbe2Type(0), channel(0),
                  digitalProbe1Type(0), digitalProbe2Type(0), digitalProbe3Type(0),
                  digitalProbe4Type(0), downSampleFactor(1), energy(0), energyShort(0),
                  flags(0), module(0), timeResolution(0), timeStampNs(0.0), waveformSize(0) {}
    
    /**
     * @brief Constructor with basic parameters
     */
    EventData(uint8_t mod, uint8_t ch, uint16_t eng, uint16_t eng_short, double timestamp) 
        : analogProbe1Type(0), analogProbe2Type(0), channel(ch),
          digitalProbe1Type(0), digitalProbe2Type(0), digitalProbe3Type(0),
          digitalProbe4Type(0), downSampleFactor(1), energy(eng), energyShort(eng_short),
          flags(0), module(mod), timeResolution(0), timeStampNs(timestamp), waveformSize(0) {}
};

} // namespace DELILA