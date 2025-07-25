/**
 * @file BinarySerializer.hpp
 * @brief Binary serialization interface for EventData structures
 * 
 * This class provides high-performance binary serialization and deserialization
 * of EventData structures with optional LZ4 compression and xxHash32 checksums.
 */

#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include "delila/net/utils/Error.hpp"
#include "delila/net/serialization/ProtocolConstants.hpp"
#include "delila/net/mock/EventData.hpp"

namespace DELILA::Net {

/**
 * @brief High-performance binary serializer for EventData structures
 * 
 * Features:
 * - Single event and batch serialization
 * - Optional LZ4 compression with configurable levels
 * - xxHash32 checksums for data integrity
 * - Thread-safe design with atomic sequence numbers
 * - Result<T> error handling pattern
 */
class BinarySerializer {
public:
    /**
     * @brief Constructor
     */
    BinarySerializer();
    
    /**
     * @brief Destructor
     */
    ~BinarySerializer();
    
    // Delete copy constructor and assignment (not thread-safe)
    BinarySerializer(const BinarySerializer&) = delete;
    BinarySerializer& operator=(const BinarySerializer&) = delete;
    
    // Allow move constructor and assignment
    BinarySerializer(BinarySerializer&&) noexcept;
    BinarySerializer& operator=(BinarySerializer&&) noexcept;
    
    /**
     * @brief Enable or disable LZ4 compression
     * @param enabled True to enable compression, false to disable
     */
    void enableCompression(bool enabled);
    
    /**
     * @brief Set LZ4 compression level
     * @param level Compression level (1-12, 1=fastest, 12=best compression)
     */
    void setCompressionLevel(int level);
    
    /**
     * @brief Calculate serialized size for a single event
     * @param event EventData to calculate size for
     * @return Size in bytes (header + waveform data)
     */
    size_t calculateEventSize(const DELILA::EventData& event) const;
    
    /**
     * @brief Serialize single EventData to binary format
     * @param event EventData to serialize
     * @return Result containing serialized binary data or error
     */
    Result<std::vector<uint8_t>> encodeEvent(const DELILA::EventData& event);
    
    /**
     * @brief Deserialize single EventData from binary format
     * @param data Binary data buffer
     * @param size Size of binary data
     * @return Result containing deserialized EventData or error
     */
    Result<DELILA::EventData> decodeEvent(const uint8_t* data, size_t size);
    
    /**
     * @brief Serialize batch of EventData to binary format with header
     * @param events Vector of EventData to serialize
     * @return Result containing serialized binary data with header or error
     */
    Result<std::vector<uint8_t>> encode(const std::vector<DELILA::EventData>& events);
    
    /**
     * @brief Deserialize batch of EventData from binary format
     * @param data Binary data buffer with header
     * @return Result containing vector of deserialized EventData or error
     */
    Result<std::vector<DELILA::EventData>> decode(const std::vector<uint8_t>& data);
    
private:
    // Configuration
    bool compression_enabled_;          ///< Whether compression is enabled
    int compression_level_;             ///< LZ4 compression level (1-12)
    
    // State management
    std::atomic<uint64_t> sequence_number_; ///< Atomic sequence counter
    
    // LZ4 compression context (opaque pointer)
    void* lz4_context_;
    
    // Performance optimization: Reusable buffers
    mutable std::vector<uint8_t> payload_buffer_;      ///< Reused for payload serialization
    mutable std::vector<uint8_t> compression_buffer_;  ///< Reused for compression operations
    mutable std::vector<uint8_t> final_buffer_;        ///< Reused for final output assembly
    
    /**
     * @brief Serialize EventData fields to buffer in alphabetical order
     * @param event EventData to serialize
     * @param buffer Output buffer (must be pre-allocated)
     * @return Status indicating success or error
     */
    Status serializeEventToBuffer(const DELILA::EventData& event, uint8_t* buffer) const;
    
    /**
     * @brief Optimized serialization using direct struct copy
     * @param event EventData to serialize
     * @param buffer Output buffer (must be pre-allocated)
     * @return Status indicating success or error
     */
    Status serializeEventToBufferFast(const DELILA::EventData& event, uint8_t* buffer) const;
    
    /**
     * @brief Deserialize EventData fields from buffer
     * @param buffer Input buffer
     * @param size Buffer size
     * @param event Output EventData structure
     * @return Status indicating success or error
     */
    Status deserializeEventFromBuffer(const uint8_t* buffer, size_t size, DELILA::EventData& event) const;
    
    /**
     * @brief Calculate xxHash32 checksum for data
     * @param data Data buffer
     * @param size Data size
     * @return 32-bit checksum
     */
    uint32_t calculateChecksum(const uint8_t* data, size_t size) const;
    
    /**
     * @brief Compress data using LZ4
     * @param input Input data
     * @param input_size Input data size
     * @param output Output buffer (must be pre-allocated)
     * @param max_output_size Maximum output buffer size
     * @return Compressed size or negative error code
     */
    int compressData(const uint8_t* input, size_t input_size, 
                     uint8_t* output, size_t max_output_size) const;
    
    /**
     * @brief Decompress data using LZ4
     * @param input Compressed input data
     * @param input_size Compressed data size
     * @param output Output buffer (must be pre-allocated)
     * @param max_output_size Maximum output buffer size
     * @return Decompressed size or negative error code
     */
    int decompressData(const uint8_t* input, size_t input_size,
                       uint8_t* output, size_t max_output_size) const;
};

} // namespace DELILA::Net