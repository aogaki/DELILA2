#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

#include "EventData.hpp"

using DELILA::Digitizer::EventData;

namespace DELILA::Net
{

struct BinaryDataHeader {
  uint64_t magic_number;       // 8 bytes: 0x44454C494C413200 ("DELILA2\0")
  uint64_t sequence_number;    // 8 bytes: for gap detection (starts at 0)
  uint32_t format_version;     // 4 bytes: binary format version (starts at 1)
  uint32_t header_size;        // 4 bytes: size of this header (always 64)
  uint32_t event_count;        // 4 bytes: number of events in payload
  uint32_t uncompressed_size;  // 4 bytes: size before compression
  uint32_t
      compressed_size;  // 4 bytes: size after compression (equals uncompressed_size if no compression)
  uint32_t checksum;     // 4 bytes: xxHash32 of payload data
  uint64_t timestamp;    // 8 bytes: Unix timestamp in nanoseconds since epoch
  uint32_t reserved[4];  // 16 bytes: future expansion
};  // Total: 64 bytes
constexpr uint32_t BINARY_DATA_HEADER_SIZE = 64;
// "DELILA2\0"
constexpr uint64_t BINARY_DATA_MAGIC_NUMBER = 0x44454C494C413200;

class Serializer
{
 public:
  Serializer();
  virtual ~Serializer();

  // Enable/disable compression and checksum
  void EnableCompression(bool enable) { fCompressionEnabled = enable; }
  void EnableChecksum(bool enable) { fChecksumEnabled = enable; }

  // Buffer pool optimization functions (Phase 3)
  void EnableBufferPool(bool enable);
  bool IsBufferPoolEnabled() const;
  void SetBufferPoolSize(size_t size);
  size_t GetBufferPoolSize() const;
  size_t GetPooledBufferCount() const;

  std::unique_ptr<std::vector<uint8_t>> Encode(
      const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events,
      uint64_t sequenceNumber = 0);

  std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> Decode(
      const std::unique_ptr<std::vector<uint8_t>> &input);

 private:
  // Add any private members or methods if needed
  bool fCompressionEnabled = true;  // Enable compression by default
  bool fChecksumEnabled = true;     // Enable checksum by default
  uint32_t fFormatVersion = 1;      // Initial format version

  // Buffer pool settings (Phase 3 optimization)
  bool fBufferPoolEnabled = true;    // Default enabled for performance
  size_t fBufferPoolSize = 20;       // Default pool size
  std::queue<std::unique_ptr<std::vector<uint8_t>>> fBufferPool;
  mutable std::mutex fBufferPoolMutex;  // Thread safety for pool access

  // Compression and checksum methods
  std::unique_ptr<std::vector<uint8_t>> Compress(
      const std::unique_ptr<std::vector<uint8_t>> &data);
  std::unique_ptr<std::vector<uint8_t>> Decompress(
      const std::unique_ptr<std::vector<uint8_t>> &data);
  
  std::unique_ptr<std::vector<uint8_t>> Decompress(
      const std::unique_ptr<std::vector<uint8_t>> &data,
      size_t uncompressedSize);

  uint32_t CalculateChecksum(const std::unique_ptr<std::vector<uint8_t>> &data);
  bool VerifyChecksum(const std::unique_ptr<std::vector<uint8_t>> &data,
                      uint32_t expectedChecksum);

  std::unique_ptr<std::vector<uint8_t>> Serialize(
      const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events);

  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> Deserialize(
      const std::unique_ptr<std::vector<uint8_t>> &input);

  // Buffer pool helper methods (Phase 3)
  std::unique_ptr<std::vector<uint8_t>> GetBufferFromPool();
  void ReturnBufferToPool(std::unique_ptr<std::vector<uint8_t>> buffer);
};

}  // namespace DELILA::Net

#endif  // SERIALIZER_HPP