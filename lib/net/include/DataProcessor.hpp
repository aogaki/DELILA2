#ifndef DATAPROCESSOR_HPP
#define DATAPROCESSOR_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

#include "../../../include/delila/core/EventData.hpp"
#include "../../../include/delila/core/MinimalEventData.hpp"

using DELILA::Digitizer::EventData;
using DELILA::Digitizer::MinimalEventData;

namespace DELILA::Net {

// Updated protocol header with compression and checksum type fields
struct BinaryDataHeader {
    uint64_t magic_number;         // 8 bytes: 0x44454C494C413200 ("DELILA2\0")
    uint64_t sequence_number;      // 8 bytes: for gap detection (starts at 0)
    uint32_t format_version;       // 4 bytes: binary format version (starts at 1)
    uint32_t header_size;          // 4 bytes: size of this header (always 64)
    uint32_t event_count;          // 4 bytes: number of events in payload
    uint32_t uncompressed_size;    // 4 bytes: size before compression
    uint32_t compressed_size;      // 4 bytes: size after compression (equals uncompressed_size if no compression)
    uint32_t checksum;             // 4 bytes: CRC32 or 0 if disabled
    uint64_t timestamp;            // 8 bytes: Unix timestamp in nanoseconds since epoch
    uint8_t  compression_type;     // 1 byte: 0=none, 1=LZ4
    uint8_t  checksum_type;        // 1 byte: 0=none, 1=CRC32
    uint8_t  reserved[14];         // 14 bytes: future use
};  // Total: 64 bytes

constexpr uint32_t BINARY_DATA_HEADER_SIZE = 64;
constexpr uint64_t BINARY_DATA_MAGIC_NUMBER = 0x44454C494C413200; // "DELILA2\0"

// Format version constants
constexpr uint32_t FORMAT_VERSION_EVENTDATA = 1;         // Full EventData with waveforms
constexpr uint32_t FORMAT_VERSION_MINIMAL_EVENTDATA = 2; // MinimalEventData (22 bytes)

// Compression type constants
constexpr uint8_t COMPRESSION_NONE = 0;
constexpr uint8_t COMPRESSION_LZ4 = 1;

// Checksum type constants  
constexpr uint8_t CHECKSUM_NONE = 0;
constexpr uint8_t CHECKSUM_CRC32 = 1;

class DataProcessor {
public:
    DataProcessor() = default;
    ~DataProcessor() = default;
    
    // Configuration methods
    void EnableCompression(bool enable = true) { compression_enabled_ = enable; }
    void EnableChecksum(bool enable = true) { checksum_enabled_ = enable; }
    bool IsCompressionEnabled() const { return compression_enabled_; }
    bool IsChecksumEnabled() const { return checksum_enabled_; }
    
    // Main processing methods
    std::unique_ptr<std::vector<uint8_t>> Process(
        const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events,
        uint64_t sequence_number);
    
    std::unique_ptr<std::vector<uint8_t>> Process(
        const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>& events,
        uint64_t sequence_number);
    
    std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> 
    Decode(const std::unique_ptr<std::vector<uint8_t>>& data);
    
    std::pair<std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>, uint64_t> 
    DecodeMinimal(const std::unique_ptr<std::vector<uint8_t>>& data);

private:
    bool compression_enabled_ = true;  // Default: LZ4 compression ON
    bool checksum_enabled_ = true;     // Default: CRC32 checksum ON
    
    // Internal processing methods
    std::unique_ptr<std::vector<uint8_t>> Serialize(
        const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events);
    
    std::unique_ptr<std::vector<uint8_t>> SerializeMinimal(
        const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>& events);
    
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> Deserialize(
        const std::unique_ptr<std::vector<uint8_t>>& data);
    
    std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> DeserializeMinimal(
        const std::unique_ptr<std::vector<uint8_t>>& data);
    
    // Compression methods
    std::unique_ptr<std::vector<uint8_t>> CompressLZ4(
        const std::unique_ptr<std::vector<uint8_t>>& data);
    
    std::unique_ptr<std::vector<uint8_t>> DecompressLZ4(
        const std::unique_ptr<std::vector<uint8_t>>& data, 
        size_t original_size);
    
public:
    // CRC32 methods (static for efficiency) - public for testing
    static uint32_t CalculateCRC32(const uint8_t* data, size_t length);
    static bool VerifyCRC32(const uint8_t* data, size_t length, uint32_t expected);
    static void InitializeCRC32Table();

private:
    
    // CRC32 lookup table
    static uint32_t crc32_table_[256];
    static bool table_initialized_;
    static constexpr uint32_t CRC32_POLYNOMIAL = 0xEDB88320;
};

} // namespace DELILA::Net

#endif // DATAPROCESSOR_HPP