/**
 * @file BinarySerializer.cpp
 * @brief Implementation of BinarySerializer class
 */

#include "delila/net/serialization/BinarySerializer.hpp"
#include "delila/net/serialization/ProtocolConstants.hpp"
#include "delila/net/utils/Platform.hpp"
#include <lz4.h>
#include <lz4hc.h>
#include <xxhash.h>
#include <cstring>
#include <algorithm>

namespace DELILA::Net {

BinarySerializer::BinarySerializer() 
    : compression_enabled_(false)
    , compression_level_(1)
    , sequence_number_(0)
    , lz4_context_(nullptr)
{
    // Initialize LZ4 compression context
    lz4_context_ = LZ4_createStream();
    
    // Pre-allocate buffers for performance
    payload_buffer_.reserve(1024 * 1024);      // 1MB initial capacity
    compression_buffer_.reserve(1024 * 1024);  // 1MB initial capacity  
    final_buffer_.reserve(1024 * 1024);        // 1MB initial capacity
}

BinarySerializer::~BinarySerializer() {
    if (lz4_context_) {
        LZ4_freeStream(static_cast<LZ4_stream_t*>(lz4_context_));
    }
}

BinarySerializer::BinarySerializer(BinarySerializer&& other) noexcept
    : compression_enabled_(other.compression_enabled_)
    , compression_level_(other.compression_level_)
    , sequence_number_(other.sequence_number_.load())
    , lz4_context_(other.lz4_context_)
    , payload_buffer_(std::move(other.payload_buffer_))
    , compression_buffer_(std::move(other.compression_buffer_))
    , final_buffer_(std::move(other.final_buffer_))
{
    other.lz4_context_ = nullptr;
}

BinarySerializer& BinarySerializer::operator=(BinarySerializer&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        if (lz4_context_) {
            LZ4_freeStream(static_cast<LZ4_stream_t*>(lz4_context_));
        }
        
        // Move from other
        compression_enabled_ = other.compression_enabled_;
        compression_level_ = other.compression_level_;
        sequence_number_ = other.sequence_number_.load();
        lz4_context_ = other.lz4_context_;
        payload_buffer_ = std::move(other.payload_buffer_);
        compression_buffer_ = std::move(other.compression_buffer_);
        final_buffer_ = std::move(other.final_buffer_);
        
        // Clear other
        other.lz4_context_ = nullptr;
    }
    return *this;
}

void BinarySerializer::enableCompression(bool enabled) {
    compression_enabled_ = enabled;
}

void BinarySerializer::setCompressionLevel(int level) {
    compression_level_ = std::clamp(level, 1, 12);
}

size_t BinarySerializer::calculateEventSize(const DELILA::EventData& event) const {
    // Calculate size based on actual waveform data, not claimed size
    // This should match SerializedEventDataHeader size (34 bytes)
    size_t header_size = 
        sizeof(event.analogProbe1Type) +    // 1 byte
        sizeof(event.analogProbe2Type) +    // 1 byte  
        sizeof(event.channel) +             // 1 byte
        sizeof(event.digitalProbe1Type) +   // 1 byte
        sizeof(event.digitalProbe2Type) +   // 1 byte
        sizeof(event.digitalProbe3Type) +   // 1 byte
        sizeof(event.digitalProbe4Type) +   // 1 byte
        sizeof(event.downSampleFactor) +    // 1 byte
        sizeof(event.energy) +              // 2 bytes
        sizeof(event.energyShort) +         // 2 bytes
        sizeof(event.flags) +               // 8 bytes
        sizeof(event.module) +              // 1 byte
        sizeof(event.timeResolution) +      // 1 byte
        sizeof(event.timeStampNs) +         // 8 bytes
        sizeof(event.waveformSize);         // 4 bytes
        // Total header: 34 bytes (matches SerializedEventDataHeader)
    
    size_t waveform_size = event.waveform.size() * sizeof(DELILA::WaveformSample);
    
    return header_size + waveform_size;
}

Result<std::vector<uint8_t>> BinarySerializer::encodeEvent(const DELILA::EventData& event) {
    try {
        size_t total_size = calculateEventSize(event);
        
        // Use reusable payload buffer for single events too
        payload_buffer_.clear();
        payload_buffer_.resize(total_size);
        
        // Serialize event to buffer using fast method
        auto serialize_result = serializeEventToBufferFast(event, payload_buffer_.data());
        if (!isOk(serialize_result)) {
            return getError(serialize_result);
        }
        
        return payload_buffer_;
    }
    catch (const std::bad_alloc&) {
        return Error{Error::MEMORY_ALLOCATION, "Failed to allocate buffer for event encoding"};
    }
    catch (...) {
        return Error{Error::SERIALIZATION_ERROR, "Unknown error during event encoding"};
    }
}

Result<DELILA::EventData> BinarySerializer::decodeEvent(const uint8_t* data, size_t size) {
    if (!data || size < 28) { // Minimum header size
        return Error{Error::INVALID_DATA, "Invalid input data for event decoding"};
    }
    
    try {
        DELILA::EventData event;
        auto deserialize_result = deserializeEventFromBuffer(data, size, event);
        if (!isOk(deserialize_result)) {
            return getError(deserialize_result);
        }
        
        return event;
    }
    catch (const std::bad_alloc&) {
        return Error{Error::MEMORY_ALLOCATION, "Failed to allocate memory for event decoding"};
    }
    catch (...) {
        return Error{Error::DESERIALIZATION_ERROR, "Unknown error during event decoding"};
    }
}

Result<std::vector<uint8_t>> BinarySerializer::encode(const std::vector<DELILA::EventData>& events) {
    if (events.empty()) {
        return Error{Error::INVALID_DATA, "Cannot encode empty event batch"};
    }
    
    try {
        // Calculate total payload size
        size_t total_payload_size = 0;
        for (const auto& event : events) {
            total_payload_size += calculateEventSize(event);
        }
        
        // Create header
        BinaryDataHeader header;
        header.magic_number = MAGIC_NUMBER;
        header.format_version = CURRENT_FORMAT_VERSION;
        header.header_size = sizeof(BinaryDataHeader);
        header.sequence_number = sequence_number_.fetch_add(1);
        header.timestamp = getCurrentTimestampNs();
        header.event_count = static_cast<uint32_t>(events.size());
        header.uncompressed_size = static_cast<uint32_t>(total_payload_size);
        header.compressed_size = static_cast<uint32_t>(total_payload_size); // Will update if compressed
        header.checksum = 0; // Will calculate after payload
        // Initialize reserved fields to zero
        for (int i = 0; i < 4; i++) {
            header.reserved[i] = 0;
        }
        
        // Serialize payload using reusable buffer
        payload_buffer_.clear();
        payload_buffer_.resize(total_payload_size);
        uint8_t* payload_ptr = payload_buffer_.data();
        
        for (const auto& event : events) {
            size_t event_size = calculateEventSize(event);
            auto serialize_result = serializeEventToBufferFast(event, payload_ptr);
            if (!isOk(serialize_result)) {
                return getError(serialize_result);
            }
            payload_ptr += event_size;
        }
        
        // Apply compression if enabled and beneficial using reusable buffer
        const uint8_t* final_payload_ptr;
        size_t final_payload_size;
        
        if (compression_enabled_ && payload_buffer_.size() >= MIN_MESSAGE_SIZE) {
            // Try to compress the payload using reusable compression buffer
            size_t max_compressed_size = LZ4_compressBound(static_cast<int>(payload_buffer_.size()));
            compression_buffer_.clear();
            compression_buffer_.resize(max_compressed_size);
            
            int compressed_size = compressData(payload_buffer_.data(), payload_buffer_.size(), 
                                             compression_buffer_.data(), compression_buffer_.size());
            
            if (compressed_size > 0 && compressed_size < static_cast<int>(payload_buffer_.size())) {
                // Compression was successful and beneficial
                compression_buffer_.resize(compressed_size);
                final_payload_ptr = compression_buffer_.data();
                final_payload_size = compressed_size;
                header.compressed_size = static_cast<uint32_t>(compressed_size);
            } else {
                // Compression failed or not beneficial, use uncompressed
                final_payload_ptr = payload_buffer_.data();
                final_payload_size = payload_buffer_.size();
                header.compressed_size = header.uncompressed_size;
            }
        } else {
            // No compression
            final_payload_ptr = payload_buffer_.data();
            final_payload_size = payload_buffer_.size();
            header.compressed_size = header.uncompressed_size;
        }
        
        // Calculate checksum on original uncompressed payload
        header.checksum = calculateChecksum(payload_buffer_.data(), payload_buffer_.size());
        
        // Prepare final buffer using reusable buffer
        size_t total_size = sizeof(BinaryDataHeader) + final_payload_size;
        final_buffer_.clear();
        final_buffer_.resize(total_size);
        
        // Copy header
        std::memcpy(final_buffer_.data(), &header, sizeof(BinaryDataHeader));
        
        // Copy final payload (compressed or uncompressed)
        std::memcpy(final_buffer_.data() + sizeof(BinaryDataHeader), final_payload_ptr, final_payload_size);
        
        return final_buffer_;
    }
    catch (const std::bad_alloc&) {
        return Error{Error::MEMORY_ALLOCATION, "Failed to allocate buffer for batch encoding"};
    }
    catch (...) {
        return Error{Error::SERIALIZATION_ERROR, "Unknown error during batch encoding"};
    }
}

Result<std::vector<DELILA::EventData>> BinarySerializer::decode(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(BinaryDataHeader)) {
        return Error{Error::INVALID_DATA, "Data too small to contain header"};
    }
    
    try {
        // Parse header
        const BinaryDataHeader* header = reinterpret_cast<const BinaryDataHeader*>(data.data());
        
        // Validate magic number
        if (header->magic_number != MAGIC_NUMBER) {
            return Error{Error::INVALID_FORMAT, "Invalid magic number in header"};
        }
        
        // Validate data size (should match header + compressed_size)
        size_t expected_size = sizeof(BinaryDataHeader) + header->compressed_size;
        if (data.size() != expected_size) {
            return Error{Error::INVALID_DATA, "Data size mismatch with header"};
        }
        
        // Extract compressed payload
        const uint8_t* compressed_payload_ptr = data.data() + sizeof(BinaryDataHeader);
        size_t compressed_payload_size = header->compressed_size;
        
        // Decompress payload if needed
        std::vector<uint8_t> uncompressed_payload;
        const uint8_t* payload_ptr;
        size_t payload_size;
        
        if (header->compressed_size < header->uncompressed_size) {
            // Data is compressed, decompress it
            uncompressed_payload.resize(header->uncompressed_size);
            int decompressed_size = decompressData(compressed_payload_ptr, compressed_payload_size,
                                                  uncompressed_payload.data(), uncompressed_payload.size());
            
            if (decompressed_size != static_cast<int>(header->uncompressed_size)) {
                return Error{Error::COMPRESSION_FAILED, "LZ4 decompression failed or size mismatch"};
            }
            
            payload_ptr = uncompressed_payload.data();
            payload_size = header->uncompressed_size;
        } else {
            // Data is not compressed
            payload_ptr = compressed_payload_ptr;
            payload_size = header->uncompressed_size;
        }
        
        // Verify checksum on uncompressed payload
        uint32_t calculated_checksum = calculateChecksum(payload_ptr, payload_size);
        if (calculated_checksum != header->checksum) {
            return Error{Error::CHECKSUM_MISMATCH, "Payload checksum verification failed"};
        }
        
        // Deserialize events
        std::vector<DELILA::EventData> events;
        events.reserve(header->event_count);
        
        const uint8_t* current_ptr = payload_ptr;
        const uint8_t* end_ptr = payload_ptr + payload_size;
        
        for (uint32_t i = 0; i < header->event_count; i++) {
            if (current_ptr >= end_ptr) {
                return Error{Error::INVALID_DATA, "Unexpected end of payload data"};
            }
            
            DELILA::EventData event;
            
            // Calculate remaining size
            size_t remaining_size = end_ptr - current_ptr;
            auto deserialize_result = deserializeEventFromBuffer(current_ptr, remaining_size, event);
            if (!isOk(deserialize_result)) {
                return getError(deserialize_result);
            }
            
            events.push_back(std::move(event));
            
            // Advance pointer
            size_t event_size = calculateEventSize(events.back());
            current_ptr += event_size;
        }
        
        return events;
    }
    catch (const std::bad_alloc&) {
        return Error{Error::MEMORY_ALLOCATION, "Failed to allocate memory for batch decoding"};
    }
    catch (...) {
        return Error{Error::DESERIALIZATION_ERROR, "Unknown error during batch decoding"};
    }
}

Status BinarySerializer::serializeEventToBuffer(const DELILA::EventData& event, uint8_t* buffer) const {
    if (!buffer) {
        return Error{Error::INVALID_DATA, "Buffer is null"};
    }
    
    // Serialize fields in alphabetical order (matching SerializedEventDataHeader)
    size_t offset = 0;
    
    // analogProbe1Type (1 byte)
    std::memcpy(buffer + offset, &event.analogProbe1Type, sizeof(event.analogProbe1Type));
    offset += sizeof(event.analogProbe1Type);
    
    // analogProbe2Type (1 byte)
    std::memcpy(buffer + offset, &event.analogProbe2Type, sizeof(event.analogProbe2Type));
    offset += sizeof(event.analogProbe2Type);
    
    // channel (1 byte)
    std::memcpy(buffer + offset, &event.channel, sizeof(event.channel));
    offset += sizeof(event.channel);
    
    // digitalProbe1Type (1 byte)
    std::memcpy(buffer + offset, &event.digitalProbe1Type, sizeof(event.digitalProbe1Type));
    offset += sizeof(event.digitalProbe1Type);
    
    // digitalProbe2Type (1 byte)
    std::memcpy(buffer + offset, &event.digitalProbe2Type, sizeof(event.digitalProbe2Type));
    offset += sizeof(event.digitalProbe2Type);
    
    // digitalProbe3Type (1 byte)
    std::memcpy(buffer + offset, &event.digitalProbe3Type, sizeof(event.digitalProbe3Type));
    offset += sizeof(event.digitalProbe3Type);
    
    // digitalProbe4Type (1 byte)
    std::memcpy(buffer + offset, &event.digitalProbe4Type, sizeof(event.digitalProbe4Type));
    offset += sizeof(event.digitalProbe4Type);
    
    // downSampleFactor (1 byte)
    std::memcpy(buffer + offset, &event.downSampleFactor, sizeof(event.downSampleFactor));
    offset += sizeof(event.downSampleFactor);
    
    // energy (2 bytes)
    std::memcpy(buffer + offset, &event.energy, sizeof(event.energy));
    offset += sizeof(event.energy);
    
    // energyShort (2 bytes)
    std::memcpy(buffer + offset, &event.energyShort, sizeof(event.energyShort));
    offset += sizeof(event.energyShort);
    
    // flags (8 bytes)
    std::memcpy(buffer + offset, &event.flags, sizeof(event.flags));
    offset += sizeof(event.flags);
    
    // module (1 byte)
    std::memcpy(buffer + offset, &event.module, sizeof(event.module));
    offset += sizeof(event.module);
    
    // timeResolution (1 byte)
    std::memcpy(buffer + offset, &event.timeResolution, sizeof(event.timeResolution));
    offset += sizeof(event.timeResolution);
    
    // timeStampNs (8 bytes)
    std::memcpy(buffer + offset, &event.timeStampNs, sizeof(event.timeStampNs));
    offset += sizeof(event.timeStampNs);
    
    // waveformSize (4 bytes) - use actual waveform size
    uint32_t actual_waveform_size = static_cast<uint32_t>(event.waveform.size());
    std::memcpy(buffer + offset, &actual_waveform_size, sizeof(actual_waveform_size));
    offset += sizeof(actual_waveform_size);
    
    // waveform data
    if (!event.waveform.empty()) {
        size_t waveform_bytes = event.waveform.size() * sizeof(DELILA::WaveformSample);
        std::memcpy(buffer + offset, event.waveform.data(), waveform_bytes);
    }
    
    return std::monostate{};
}

Status BinarySerializer::serializeEventToBufferFast(const DELILA::EventData& event, uint8_t* buffer) const {
    if (!buffer) {
        return Error{Error::INVALID_DATA, "Buffer is null"};
    }
    
    // Create a packed structure for fast serialization
    // This directly maps to the SerializedEventDataHeader layout
    struct __attribute__((packed)) FastEventHeader {
        uint8_t analogProbe1Type;
        uint8_t analogProbe2Type;
        uint8_t channel;
        uint8_t digitalProbe1Type;
        uint8_t digitalProbe2Type;
        uint8_t digitalProbe3Type;
        uint8_t digitalProbe4Type;
        uint8_t downSampleFactor;
        uint16_t energy;
        uint16_t energyShort;
        uint64_t flags;
        uint8_t module;
        uint8_t timeResolution;
        double timeStampNs;
        uint32_t waveformSize;
    };
    
    // Fill the fast header structure
    FastEventHeader header;
    header.analogProbe1Type = event.analogProbe1Type;
    header.analogProbe2Type = event.analogProbe2Type;
    header.channel = event.channel;
    header.digitalProbe1Type = event.digitalProbe1Type;
    header.digitalProbe2Type = event.digitalProbe2Type;
    header.digitalProbe3Type = event.digitalProbe3Type;
    header.digitalProbe4Type = event.digitalProbe4Type;
    header.downSampleFactor = event.downSampleFactor;
    header.energy = event.energy;
    header.energyShort = event.energyShort;
    header.flags = event.flags;
    header.module = event.module;
    header.timeResolution = event.timeResolution;
    header.timeStampNs = event.timeStampNs;
    header.waveformSize = static_cast<uint32_t>(event.waveform.size());
    
    // Single memcpy for the entire header (34 bytes)
    std::memcpy(buffer, &header, sizeof(header));
    
    // Copy waveform data in one operation if present
    if (!event.waveform.empty()) {
        size_t waveform_bytes = event.waveform.size() * sizeof(DELILA::WaveformSample);
        std::memcpy(buffer + sizeof(header), event.waveform.data(), waveform_bytes);
    }
    
    return std::monostate{};
}

Status BinarySerializer::deserializeEventFromBuffer(const uint8_t* buffer, size_t size, 
                                                          DELILA::EventData& event) const {
    if (!buffer || size < 34) { // Minimum header size (SerializedEventDataHeader)
        return Error{Error::INVALID_DATA, "Invalid buffer for event deserialization"};
    }
    
    // Deserialize fields in alphabetical order (matching SerializedEventDataHeader)
    size_t offset = 0;
    
    // analogProbe1Type (1 byte)
    std::memcpy(&event.analogProbe1Type, buffer + offset, sizeof(event.analogProbe1Type));
    offset += sizeof(event.analogProbe1Type);
    
    // analogProbe2Type (1 byte)
    std::memcpy(&event.analogProbe2Type, buffer + offset, sizeof(event.analogProbe2Type));
    offset += sizeof(event.analogProbe2Type);
    
    // channel (1 byte)
    std::memcpy(&event.channel, buffer + offset, sizeof(event.channel));
    offset += sizeof(event.channel);
    
    // digitalProbe1Type (1 byte)
    std::memcpy(&event.digitalProbe1Type, buffer + offset, sizeof(event.digitalProbe1Type));
    offset += sizeof(event.digitalProbe1Type);
    
    // digitalProbe2Type (1 byte)
    std::memcpy(&event.digitalProbe2Type, buffer + offset, sizeof(event.digitalProbe2Type));
    offset += sizeof(event.digitalProbe2Type);
    
    // digitalProbe3Type (1 byte)
    std::memcpy(&event.digitalProbe3Type, buffer + offset, sizeof(event.digitalProbe3Type));
    offset += sizeof(event.digitalProbe3Type);
    
    // digitalProbe4Type (1 byte)
    std::memcpy(&event.digitalProbe4Type, buffer + offset, sizeof(event.digitalProbe4Type));
    offset += sizeof(event.digitalProbe4Type);
    
    // downSampleFactor (1 byte)
    std::memcpy(&event.downSampleFactor, buffer + offset, sizeof(event.downSampleFactor));
    offset += sizeof(event.downSampleFactor);
    
    // energy (2 bytes)
    std::memcpy(&event.energy, buffer + offset, sizeof(event.energy));
    offset += sizeof(event.energy);
    
    // energyShort (2 bytes)
    std::memcpy(&event.energyShort, buffer + offset, sizeof(event.energyShort));
    offset += sizeof(event.energyShort);
    
    // flags (8 bytes)
    std::memcpy(&event.flags, buffer + offset, sizeof(event.flags));
    offset += sizeof(event.flags);
    
    // module (1 byte)
    std::memcpy(&event.module, buffer + offset, sizeof(event.module));
    offset += sizeof(event.module);
    
    // timeResolution (1 byte)
    std::memcpy(&event.timeResolution, buffer + offset, sizeof(event.timeResolution));
    offset += sizeof(event.timeResolution);
    
    // timeStampNs (8 bytes)
    std::memcpy(&event.timeStampNs, buffer + offset, sizeof(event.timeStampNs));
    offset += sizeof(event.timeStampNs);
    
    // waveformSize (4 bytes)
    std::memcpy(&event.waveformSize, buffer + offset, sizeof(event.waveformSize));
    offset += sizeof(event.waveformSize);
    
    // Validate remaining buffer size for waveform
    size_t waveform_bytes = event.waveformSize * sizeof(DELILA::WaveformSample);
    if (offset + waveform_bytes > size) {
        return Error{Error::INVALID_DATA, "Buffer too small for waveform data"};
    }
    
    // waveform data
    event.waveform.clear();
    if (event.waveformSize > 0) {
        event.waveform.resize(event.waveformSize);
        std::memcpy(event.waveform.data(), buffer + offset, waveform_bytes);
    }
    
    return std::monostate{};
}

uint32_t BinarySerializer::calculateChecksum(const uint8_t* data, size_t size) const {
    return XXH32(data, size, 0); // Use seed 0
}

int BinarySerializer::compressData(const uint8_t* input, size_t input_size,
                                   uint8_t* output, size_t max_output_size) const {
    if (!input || !output || input_size == 0) {
        return -1; // Invalid parameters
    }
    
    // Use LZ4_compress_fast with acceleration parameter
    // Higher acceleration = faster compression, lower ratio
    int acceleration = (compression_level_ <= 1) ? 1 : (10 - compression_level_);
    
    return LZ4_compress_fast(reinterpret_cast<const char*>(input), 
                            reinterpret_cast<char*>(output),
                            static_cast<int>(input_size),
                            static_cast<int>(max_output_size),
                            acceleration);
}

int BinarySerializer::decompressData(const uint8_t* input, size_t input_size,
                                     uint8_t* output, size_t max_output_size) const {
    return LZ4_decompress_safe(reinterpret_cast<const char*>(input),
                              reinterpret_cast<char*>(output),
                              static_cast<int>(input_size),
                              static_cast<int>(max_output_size));
}

} // namespace DELILA::Net