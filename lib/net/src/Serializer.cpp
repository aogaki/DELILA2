#include "Serializer.hpp"

#include <chrono>
#include <cstring>
#include <algorithm>
#include <lz4.h>
#include <lz4hc.h>
#include <xxhash.h>

namespace DELILA::Net
{

// Size of the BinaryDataHeader structure

Serializer::Serializer() = default;

Serializer::~Serializer() = default;

std::unique_ptr<std::vector<uint8_t>> Serializer::Encode(
    const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events,
    uint64_t sequenceNumber)
{
  if (!events) {
    return nullptr;
  }
  
  // Create header
  BinaryDataHeader header{};
  header.magic_number = BINARY_DATA_MAGIC_NUMBER;
  header.sequence_number = sequenceNumber;
  header.format_version = fFormatVersion;
  header.header_size = BINARY_DATA_HEADER_SIZE;
  header.event_count = events->size();
  header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  // Serialize event data using buffer pool for intermediate work
  auto serializedData = Serialize(events);
  if (!serializedData) {
    return nullptr;
  }
  
  header.uncompressed_size = serializedData->size();
  
  // Optionally compress data
  std::unique_ptr<std::vector<uint8_t>>* dataToEncode = &serializedData;
  std::unique_ptr<std::vector<uint8_t>> compressedData;
  if (fCompressionEnabled && !serializedData->empty()) {
    compressedData = Compress(serializedData);
    if (compressedData && compressedData->size() < serializedData->size()) {
      dataToEncode = &compressedData;
      header.compressed_size = compressedData->size();
    } else {
      header.compressed_size = header.uncompressed_size;
    }
  } else {
    header.compressed_size = header.uncompressed_size;
  }
  
  // Calculate checksum if enabled
  if (fChecksumEnabled && *dataToEncode) {
    header.checksum = CalculateChecksum(*dataToEncode);
  } else {
    header.checksum = 0;
  }
  
  // Clear reserved fields
  std::memset(header.reserved, 0, sizeof(header.reserved));
  
  // Create final output using buffer pool (Phase 3 optimization)
  auto result = GetBufferFromPool();
  result->reserve(sizeof(header) + (*dataToEncode ? (*dataToEncode)->size() : 0));
  
  // Write header
  const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
  result->insert(result->end(), headerBytes, headerBytes + sizeof(header));
  
  // Write data
  if (*dataToEncode && !(*dataToEncode)->empty()) {
    result->insert(result->end(), (*dataToEncode)->begin(), (*dataToEncode)->end());
  }
  
  return result;
}

std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> Serializer::Decode(
    const std::unique_ptr<std::vector<uint8_t>> &input)
{
  if (!input || input->size() < sizeof(BinaryDataHeader)) {
    return std::make_pair(nullptr, 0);
  }
  
  // Read header
  BinaryDataHeader header;
  std::memcpy(&header, input->data(), sizeof(header));
  
  // Validate header
  if (header.magic_number != BINARY_DATA_MAGIC_NUMBER) {
    return std::make_pair(nullptr, 0);
  }
  
  if (header.header_size != BINARY_DATA_HEADER_SIZE) {
    return std::make_pair(nullptr, 0);
  }
  
  if (header.format_version != fFormatVersion) {
    // TODO: Handle version mismatch or implement version compatibility
    return std::make_pair(nullptr, 0);
  }
  
  // Extract payload
  if (input->size() < sizeof(header) + header.compressed_size) {
    return std::make_pair(nullptr, 0);
  }
  
  auto payload = std::make_unique<std::vector<uint8_t>>(
      input->begin() + sizeof(header), 
      input->begin() + sizeof(header) + header.compressed_size);
  
  // Verify checksum if enabled
  if (fChecksumEnabled && header.checksum != 0) {
    if (!VerifyChecksum(payload, header.checksum)) {
      return std::make_pair(nullptr, 0);
    }
  }
  
  // Decompress if needed
  std::unique_ptr<std::vector<uint8_t>>* dataToDeserialize = &payload;
  std::unique_ptr<std::vector<uint8_t>> decompressedData;
  if (header.compressed_size < header.uncompressed_size) {
    decompressedData = Decompress(payload, header.uncompressed_size);
    if (!decompressedData || decompressedData->size() != header.uncompressed_size) {
      return std::make_pair(nullptr, 0);
    }
    dataToDeserialize = &decompressedData;
  }
  
  // Deserialize events
  auto events = Deserialize(*dataToDeserialize);
  
  // Validate event count
  if (!events || events->size() != header.event_count) {
    return std::make_pair(nullptr, 0);
  }
  
  return std::make_pair(std::move(events), header.sequence_number);
}

std::unique_ptr<std::vector<uint8_t>> Serializer::Compress(
    const std::unique_ptr<std::vector<uint8_t>> &data)
{
  if (!data || data->empty()) {
    return nullptr;
  }
  
  const int srcSize = static_cast<int>(data->size());
  const int maxCompressedSize = LZ4_compressBound(srcSize);
  
  auto compressed = GetBufferFromPool();  // Phase 3: Use buffer pool
  compressed->resize(maxCompressedSize);
  
  const int compressedSize = LZ4_compress_default(
      reinterpret_cast<const char*>(data->data()),
      reinterpret_cast<char*>(compressed->data()),
      srcSize,
      maxCompressedSize);
  
  if (compressedSize <= 0) {
    // Compression failed - return buffer to pool
    ReturnBufferToPool(std::move(compressed));
    return nullptr;
  }
  
  // Resize to actual compressed size
  compressed->resize(compressedSize);
  
  // Only return compressed data if it's actually smaller
  if (static_cast<size_t>(compressedSize) < data->size()) {
    return compressed;
  }
  
  // If compression didn't save space, return buffer to pool
  ReturnBufferToPool(std::move(compressed));
  return nullptr;
}
std::unique_ptr<std::vector<uint8_t>> Serializer::Decompress(
    const std::unique_ptr<std::vector<uint8_t>> &data)
{
  if (!data || data->empty()) {
    return nullptr;
  }
  
  // Try common multipliers for decompression when size is unknown
  const std::vector<int> multipliers = {2, 4, 8, 16, 32};
  
  for (int multiplier : multipliers) {
    const int decompressedSize = static_cast<int>(data->size()) * multiplier;
    auto decompressed = GetBufferFromPool();  // Phase 3: Use buffer pool
    decompressed->resize(decompressedSize);
    
    const int result = LZ4_decompress_safe(
        reinterpret_cast<const char*>(data->data()),
        reinterpret_cast<char*>(decompressed->data()),
        static_cast<int>(data->size()),
        decompressedSize);
    
    if (result > 0) {
      // Decompression successful, resize to actual size
      decompressed->resize(result);
      return decompressed;
    }
    
    // This attempt failed - return buffer to pool and try next multiplier
    ReturnBufferToPool(std::move(decompressed));
  }
  
  // Decompression failed with all attempted sizes
  return nullptr;
}

std::unique_ptr<std::vector<uint8_t>> Serializer::Decompress(
    const std::unique_ptr<std::vector<uint8_t>> &data,
    size_t uncompressedSize)
{
  if (!data || data->empty() || uncompressedSize == 0) {
    return nullptr;
  }
  
  auto decompressed = GetBufferFromPool();  // Phase 3: Use buffer pool
  decompressed->resize(uncompressedSize);
  
  const int result = LZ4_decompress_safe(
      reinterpret_cast<const char*>(data->data()),
      reinterpret_cast<char*>(decompressed->data()),
      static_cast<int>(data->size()),
      static_cast<int>(uncompressedSize));
  
  if (result > 0 && static_cast<size_t>(result) == uncompressedSize) {
    return decompressed;
  }
  
  // Decompression failed or size mismatch - return buffer to pool
  ReturnBufferToPool(std::move(decompressed));
  return nullptr;
}

uint32_t Serializer::CalculateChecksum(
    const std::unique_ptr<std::vector<uint8_t>> &data)
{
  if (!data || data->empty()) {
    return 0;
  }
  
  // Use xxHash32 with seed 0 for deterministic results
  const uint32_t seed = 0;
  return XXH32(data->data(), data->size(), seed);
}
bool Serializer::VerifyChecksum(
    const std::unique_ptr<std::vector<uint8_t>> &data,
    uint32_t expectedChecksum)
{
  uint32_t calculatedChecksum = CalculateChecksum(data);
  return calculatedChecksum == expectedChecksum;
}

std::unique_ptr<std::vector<uint8_t>> Serializer::Serialize(
    const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events)
{
  if (!events || events->empty()) {
    return GetBufferFromPool();  // Return empty buffer from pool
  }

  auto result = GetBufferFromPool();  // Phase 3: Use buffer pool
  
  // Reserve approximate space to avoid reallocations
  size_t approxSize = events->size() * (Digitizer::EVENTDATA_SIZE + 1000); // Extra space for waveforms
  result->reserve(approxSize);
  
  // Serialize each event
  for (const auto& event : *events) {
    if (!event) continue;
    
    // Write fixed-size fields
    auto appendBytes = [&result](const void* data, size_t size) {
      const uint8_t* bytes = static_cast<const uint8_t*>(data);
      result->insert(result->end(), bytes, bytes + size);
    };
    
    appendBytes(&event->timeStampNs, sizeof(event->timeStampNs));
    appendBytes(&event->waveformSize, sizeof(event->waveformSize));
    appendBytes(&event->energy, sizeof(event->energy));
    appendBytes(&event->energyShort, sizeof(event->energyShort));
    appendBytes(&event->module, sizeof(event->module));
    appendBytes(&event->channel, sizeof(event->channel));
    appendBytes(&event->timeResolution, sizeof(event->timeResolution));
    appendBytes(&event->analogProbe1Type, sizeof(event->analogProbe1Type));
    appendBytes(&event->analogProbe2Type, sizeof(event->analogProbe2Type));
    appendBytes(&event->digitalProbe1Type, sizeof(event->digitalProbe1Type));
    appendBytes(&event->digitalProbe2Type, sizeof(event->digitalProbe2Type));
    appendBytes(&event->digitalProbe3Type, sizeof(event->digitalProbe3Type));
    appendBytes(&event->digitalProbe4Type, sizeof(event->digitalProbe4Type));
    appendBytes(&event->downSampleFactor, sizeof(event->downSampleFactor));
    appendBytes(&event->flags, sizeof(event->flags));
    
    // Write variable-size waveform data
    // Analog probe 1
    uint32_t size = event->analogProbe1.size();
    appendBytes(&size, sizeof(size));
    if (size > 0) {
      appendBytes(event->analogProbe1.data(), size * sizeof(int32_t));
    }
    
    // Analog probe 2
    size = event->analogProbe2.size();
    appendBytes(&size, sizeof(size));
    if (size > 0) {
      appendBytes(event->analogProbe2.data(), size * sizeof(int32_t));
    }
    
    // Digital probes (1-4)
    size = event->digitalProbe1.size();
    appendBytes(&size, sizeof(size));
    if (size > 0) {
      appendBytes(event->digitalProbe1.data(), size * sizeof(uint8_t));
    }
    
    size = event->digitalProbe2.size();
    appendBytes(&size, sizeof(size));
    if (size > 0) {
      appendBytes(event->digitalProbe2.data(), size * sizeof(uint8_t));
    }
    
    size = event->digitalProbe3.size();
    appendBytes(&size, sizeof(size));
    if (size > 0) {
      appendBytes(event->digitalProbe3.data(), size * sizeof(uint8_t));
    }
    
    size = event->digitalProbe4.size();
    appendBytes(&size, sizeof(size));
    if (size > 0) {
      appendBytes(event->digitalProbe4.data(), size * sizeof(uint8_t));
    }
  }
  
  return result;
}
std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
Serializer::Deserialize(const std::unique_ptr<std::vector<uint8_t>> &input)
{
  auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
  
  if (!input || input->empty()) {
    return events;
  }
  
  size_t offset = 0;
  const uint8_t* data = input->data();
  size_t dataSize = input->size();
  
  // Helper to read bytes
  auto readBytes = [&data, &offset, &dataSize](void* dest, size_t size) -> bool {
    if (offset + size > dataSize) {
      return false;
    }
    std::memcpy(dest, data + offset, size);
    offset += size;
    return true;
  };
  
  while (offset < dataSize) {
    auto event = std::make_unique<EventData>();
    
    // Read fixed-size fields
    if (!readBytes(&event->timeStampNs, sizeof(event->timeStampNs))) break;
    if (!readBytes(&event->waveformSize, sizeof(event->waveformSize))) break;
    if (!readBytes(&event->energy, sizeof(event->energy))) break;
    if (!readBytes(&event->energyShort, sizeof(event->energyShort))) break;
    if (!readBytes(&event->module, sizeof(event->module))) break;
    if (!readBytes(&event->channel, sizeof(event->channel))) break;
    if (!readBytes(&event->timeResolution, sizeof(event->timeResolution))) break;
    if (!readBytes(&event->analogProbe1Type, sizeof(event->analogProbe1Type))) break;
    if (!readBytes(&event->analogProbe2Type, sizeof(event->analogProbe2Type))) break;
    if (!readBytes(&event->digitalProbe1Type, sizeof(event->digitalProbe1Type))) break;
    if (!readBytes(&event->digitalProbe2Type, sizeof(event->digitalProbe2Type))) break;
    if (!readBytes(&event->digitalProbe3Type, sizeof(event->digitalProbe3Type))) break;
    if (!readBytes(&event->digitalProbe4Type, sizeof(event->digitalProbe4Type))) break;
    if (!readBytes(&event->downSampleFactor, sizeof(event->downSampleFactor))) break;
    if (!readBytes(&event->flags, sizeof(event->flags))) break;
    
    // Read variable-size waveform data
    uint32_t size;
    
    // Analog probe 1
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->analogProbe1.resize(size);
      if (!readBytes(event->analogProbe1.data(), size * sizeof(int32_t))) break;
    }
    
    // Analog probe 2
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->analogProbe2.resize(size);
      if (!readBytes(event->analogProbe2.data(), size * sizeof(int32_t))) break;
    }
    
    // Digital probe 1
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe1.resize(size);
      if (!readBytes(event->digitalProbe1.data(), size * sizeof(uint8_t))) break;
    }
    
    // Digital probe 2
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe2.resize(size);
      if (!readBytes(event->digitalProbe2.data(), size * sizeof(uint8_t))) break;
    }
    
    // Digital probe 3
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe3.resize(size);
      if (!readBytes(event->digitalProbe3.data(), size * sizeof(uint8_t))) break;
    }
    
    // Digital probe 4
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe4.resize(size);
      if (!readBytes(event->digitalProbe4.data(), size * sizeof(uint8_t))) break;
    }
    
    events->push_back(std::move(event));
  }
  
  return events;
}

// ==========================================
// Buffer Pool Optimization Implementation (Phase 3)
// ==========================================

void Serializer::EnableBufferPool(bool enable)
{
  fBufferPoolEnabled = enable;
}

bool Serializer::IsBufferPoolEnabled() const
{
  return fBufferPoolEnabled;
}

void Serializer::SetBufferPoolSize(size_t size)
{
  if (size == 0) {
    fBufferPoolSize = 20;  // Reset to default
  } else {
    fBufferPoolSize = size;
  }
}

size_t Serializer::GetBufferPoolSize() const
{
  return fBufferPoolSize;
}

size_t Serializer::GetPooledBufferCount() const
{
  std::lock_guard<std::mutex> lock(fBufferPoolMutex);
  return fBufferPool.size();
}

std::unique_ptr<std::vector<uint8_t>> Serializer::GetBufferFromPool()
{
  if (!fBufferPoolEnabled) {
    // If pool is disabled, always return a new buffer
    return std::make_unique<std::vector<uint8_t>>();
  }
  
  std::lock_guard<std::mutex> lock(fBufferPoolMutex);
  
  if (!fBufferPool.empty()) {
    // Get buffer from pool
    auto buffer = std::move(fBufferPool.front());
    fBufferPool.pop();
    
    // Clear the buffer for reuse
    buffer->clear();
    return buffer;
  }
  
  // Pool is empty, create new buffer
  return std::make_unique<std::vector<uint8_t>>();
}

void Serializer::ReturnBufferToPool(std::unique_ptr<std::vector<uint8_t>> buffer)
{
  if (!fBufferPoolEnabled || !buffer) {
    // If pool is disabled or buffer is null, just let it be destroyed
    return;
  }
  
  std::lock_guard<std::mutex> lock(fBufferPoolMutex);
  
  // Only return to pool if we haven't exceeded the maximum pool size
  if (fBufferPool.size() < fBufferPoolSize) {
    // Clear the buffer and return to pool
    buffer->clear();
    fBufferPool.push(std::move(buffer));
  }
  // If pool is full, let the buffer be destroyed naturally
}

}  // namespace DELILA::Net