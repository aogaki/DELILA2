#include "../include/DataProcessor.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace DELILA::Net
{

// Static member initialization
uint32_t DataProcessor::crc32_table_[256];
bool DataProcessor::table_initialized_ = false;

// CRC32 implementation
void DataProcessor::InitializeCRC32Table()
{
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t crc = i;
    for (int j = 0; j < 8; ++j) {
      if (crc & 1) {
        crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
      } else {
        crc >>= 1;
      }
    }
    crc32_table_[i] = crc;
  }
  table_initialized_ = true;
}

uint32_t DataProcessor::CalculateCRC32(const uint8_t *data, size_t length)
{
  if (!table_initialized_) {
    InitializeCRC32Table();
  }

  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc = crc32_table_[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFF;
}

bool DataProcessor::VerifyCRC32(const uint8_t *data, size_t length,
                                uint32_t expected)
{
  return CalculateCRC32(data, length) == expected;
}

// Main processing methods - full pipeline implementation
std::unique_ptr<std::vector<uint8_t>> DataProcessor::Process(
    const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events,
    uint64_t sequence_number)
{
  if (!events) {
    return nullptr;
  }

  // Create header
  BinaryDataHeader header{};
  header.magic_number = BINARY_DATA_MAGIC_NUMBER;
  header.sequence_number = sequence_number;
  header.format_version = FORMAT_VERSION_EVENTDATA;
  header.header_size = BINARY_DATA_HEADER_SIZE;
  header.event_count = events->size();
  header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

  // Compression is disabled - always NONE
  header.compression_type = COMPRESSION_NONE;
  header.checksum_type = checksum_enabled_ ? CHECKSUM_CRC32 : CHECKSUM_NONE;
  header.message_type = MESSAGE_TYPE_DATA;

  // Serialize event data
  auto serializedData = Serialize(events);
  if (!serializedData) {
    return nullptr;
  }

  header.uncompressed_size = serializedData->size();
  header.compressed_size = serializedData->size();  // No compression

  // Calculate checksum if enabled
  if (checksum_enabled_) {
    header.checksum =
        CalculateCRC32(serializedData->data(), serializedData->size());
  } else {
    header.checksum = 0;
  }

  // Clear reserved fields
  std::memset(header.reserved, 0, sizeof(header.reserved));

  // Create final output
  auto result = std::make_unique<std::vector<uint8_t>>();
  result->reserve(sizeof(header) + serializedData->size());

  // Write header
  const uint8_t *headerBytes = reinterpret_cast<const uint8_t *>(&header);
  result->insert(result->end(), headerBytes, headerBytes + sizeof(header));

  // Write data
  if (!serializedData->empty()) {
    result->insert(result->end(), serializedData->begin(),
                   serializedData->end());
  }

  return result;
}

std::unique_ptr<std::vector<uint8_t>> DataProcessor::Process(
    const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>
        &events,
    uint64_t sequence_number)
{
  if (!events) {
    return nullptr;
  }

  // Create header
  BinaryDataHeader header{};
  header.magic_number = BINARY_DATA_MAGIC_NUMBER;
  header.sequence_number = sequence_number;
  header.format_version = FORMAT_VERSION_MINIMAL_EVENTDATA;
  header.header_size = BINARY_DATA_HEADER_SIZE;
  header.event_count = events->size();
  header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

  // Compression is disabled - always NONE
  header.compression_type = COMPRESSION_NONE;
  header.checksum_type = checksum_enabled_ ? CHECKSUM_CRC32 : CHECKSUM_NONE;
  header.message_type = MESSAGE_TYPE_DATA;

  // Serialize event data
  auto serializedData = SerializeMinimal(events);
  if (!serializedData) {
    return nullptr;
  }

  header.uncompressed_size = serializedData->size();
  header.compressed_size = serializedData->size();  // No compression

  // Calculate checksum if enabled
  if (checksum_enabled_) {
    header.checksum =
        CalculateCRC32(serializedData->data(), serializedData->size());
  } else {
    header.checksum = 0;
  }

  // Clear reserved fields
  std::memset(header.reserved, 0, sizeof(header.reserved));

  // Create final output
  auto result = std::make_unique<std::vector<uint8_t>>();
  result->reserve(sizeof(header) + serializedData->size());

  // Write header
  const uint8_t *headerBytes = reinterpret_cast<const uint8_t *>(&header);
  result->insert(result->end(), headerBytes, headerBytes + sizeof(header));

  // Write data
  if (!serializedData->empty()) {
    result->insert(result->end(), serializedData->begin(),
                   serializedData->end());
  }

  return result;
}

std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t>
DataProcessor::Decode(const std::unique_ptr<std::vector<uint8_t>> &data)
{
  // Handle null/empty input
  if (!data || data->empty()) {
    return {nullptr, 0};
  }

  // Check minimum size for header
  if (data->size() < sizeof(BinaryDataHeader)) {
    return {nullptr, 0};
  }

  // Parse header
  const BinaryDataHeader *header =
      reinterpret_cast<const BinaryDataHeader *>(data->data());

  // Validate magic number
  if (header->magic_number != BINARY_DATA_MAGIC_NUMBER) {
    return {nullptr, 0};
  }

  // Validate format version
  if (header->format_version != FORMAT_VERSION_EVENTDATA) {
    return {nullptr, 0};
  }

  // Validate header size
  if (header->header_size != BINARY_DATA_HEADER_SIZE) {
    return {nullptr, 0};
  }

  // Extract sequence number
  uint64_t sequence_number = header->sequence_number;

  // Get payload (after header) - no compression, use uncompressed_size
  if (data->size() < sizeof(BinaryDataHeader) + header->uncompressed_size) {
    return {nullptr, 0};  // Size mismatch
  }

  auto payload = std::make_unique<std::vector<uint8_t>>(
      data->begin() + sizeof(BinaryDataHeader),
      data->begin() + sizeof(BinaryDataHeader) + header->uncompressed_size);

  // CRC32 verification (conditional)
  if (checksum_enabled_ && header->checksum_type == CHECKSUM_CRC32) {
    if (!VerifyCRC32(payload->data(), payload->size(), header->checksum)) {
      return {nullptr, 0};  // Checksum verification failed
    }
  }

  // Deserialization to EventData
  auto events = Deserialize(payload);
  if (!events) {
    return {nullptr, 0};  // Deserialization failed
  }

  return {std::move(events), sequence_number};
}

std::pair<std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>,
          uint64_t>
DataProcessor::DecodeMinimal(const std::unique_ptr<std::vector<uint8_t>> &data)
{
  // Handle null/empty input
  if (!data || data->empty()) {
    return {nullptr, 0};
  }

  // Check minimum size for header
  if (data->size() < sizeof(BinaryDataHeader)) {
    return {nullptr, 0};
  }

  // Parse header
  const BinaryDataHeader *header =
      reinterpret_cast<const BinaryDataHeader *>(data->data());

  // Validate magic number
  if (header->magic_number != BINARY_DATA_MAGIC_NUMBER) {
    return {nullptr, 0};
  }

  // Validate format version (accept both EventData and MinimalEventData)
  if (header->format_version != FORMAT_VERSION_EVENTDATA &&
      header->format_version != FORMAT_VERSION_MINIMAL_EVENTDATA) {
    return {nullptr, 0};
  }

  // Validate header size
  if (header->header_size != BINARY_DATA_HEADER_SIZE) {
    return {nullptr, 0};
  }

  // Extract sequence number
  uint64_t sequence_number = header->sequence_number;

  // Get payload (after header) - no compression, use uncompressed_size
  if (data->size() < sizeof(BinaryDataHeader) + header->uncompressed_size) {
    return {nullptr, 0};  // Size mismatch
  }

  auto payload = std::make_unique<std::vector<uint8_t>>(
      data->begin() + sizeof(BinaryDataHeader),
      data->begin() + sizeof(BinaryDataHeader) + header->uncompressed_size);

  // CRC32 verification (conditional)
  if (checksum_enabled_ && header->checksum_type == CHECKSUM_CRC32) {
    if (!VerifyCRC32(payload->data(), payload->size(), header->checksum)) {
      return {nullptr, 0};  // Checksum verification failed
    }
  }

  // Deserialization to MinimalEventData
  auto events = DeserializeMinimal(payload);
  if (!events) {
    return {nullptr, 0};  // Deserialization failed
  }

  return {std::move(events), sequence_number};
}

// Internal methods - serialization implementation (copied from Serializer, simplified)
std::unique_ptr<std::vector<uint8_t>> DataProcessor::Serialize(
    const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events)
{
  if (!events || events->empty()) {
    return std::make_unique<
        std::vector<uint8_t>>();  // Return empty buffer (no pool)
  }

  auto result = std::make_unique<std::vector<uint8_t>>();

  // Reserve approximate space to avoid reallocations
  size_t approxSize = events->size() * (Digitizer::EVENTDATA_SIZE +
                                        1000);  // Extra space for waveforms
  result->reserve(approxSize);

  // Serialize each event
  for (const auto &event : *events) {
    if (!event) continue;

    // Write fixed-size fields
    auto appendBytes = [&result](const void *data, size_t size) {
      const uint8_t *bytes = static_cast<const uint8_t *>(data);
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
    appendBytes(&event->aMax, sizeof(event->aMax));

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

std::unique_ptr<std::vector<uint8_t>> DataProcessor::SerializeMinimal(
    const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>
        &events)
{
  if (!events || events->empty()) {
    return std::make_unique<
        std::vector<uint8_t>>();  // Return empty buffer (no pool)
  }

  auto result = std::make_unique<std::vector<uint8_t>>();

  // Reserve exact space needed: each MinimalEventData is exactly 22 bytes
  constexpr size_t MINIMAL_EVENT_SIZE = 22;
  result->reserve(events->size() * MINIMAL_EVENT_SIZE);

  // Serialize each event as fixed 22-byte record
  for (const auto &event : *events) {
    if (!event) continue;

    // Simply copy the packed struct as binary data
    const uint8_t *eventBytes = reinterpret_cast<const uint8_t *>(event.get());
    result->insert(result->end(), eventBytes, eventBytes + MINIMAL_EVENT_SIZE);
  }

  return result;
}

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
DataProcessor::Deserialize(const std::unique_ptr<std::vector<uint8_t>> &data)
{
  auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();

  if (!data || data->empty()) {
    return events;
  }

  size_t offset = 0;
  const uint8_t *rawData = data->data();
  size_t dataSize = data->size();

  // Helper to read bytes
  auto readBytes = [&rawData, &offset, &dataSize](void *dest,
                                                  size_t size) -> bool {
    if (offset + size > dataSize) {
      return false;
    }
    std::memcpy(dest, rawData + offset, size);
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
    if (!readBytes(&event->timeResolution, sizeof(event->timeResolution)))
      break;
    if (!readBytes(&event->analogProbe1Type, sizeof(event->analogProbe1Type)))
      break;
    if (!readBytes(&event->analogProbe2Type, sizeof(event->analogProbe2Type)))
      break;
    if (!readBytes(&event->digitalProbe1Type, sizeof(event->digitalProbe1Type)))
      break;
    if (!readBytes(&event->digitalProbe2Type, sizeof(event->digitalProbe2Type)))
      break;
    if (!readBytes(&event->digitalProbe3Type, sizeof(event->digitalProbe3Type)))
      break;
    if (!readBytes(&event->digitalProbe4Type, sizeof(event->digitalProbe4Type)))
      break;
    if (!readBytes(&event->downSampleFactor, sizeof(event->downSampleFactor)))
      break;
    if (!readBytes(&event->flags, sizeof(event->flags))) break;
    if (!readBytes(&event->aMax, sizeof(event->aMax))) break;

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
      if (!readBytes(event->digitalProbe1.data(), size * sizeof(uint8_t)))
        break;
    }

    // Digital probe 2
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe2.resize(size);
      if (!readBytes(event->digitalProbe2.data(), size * sizeof(uint8_t)))
        break;
    }

    // Digital probe 3
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe3.resize(size);
      if (!readBytes(event->digitalProbe3.data(), size * sizeof(uint8_t)))
        break;
    }

    // Digital probe 4
    if (!readBytes(&size, sizeof(size))) break;
    if (size > 0) {
      event->digitalProbe4.resize(size);
      if (!readBytes(event->digitalProbe4.data(), size * sizeof(uint8_t)))
        break;
    }

    events->push_back(std::move(event));
  }

  return events;
}

std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>
DataProcessor::DeserializeMinimal(
    const std::unique_ptr<std::vector<uint8_t>> &data)
{
  if (!data || data->empty()) {
    return std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
  }

  constexpr size_t MINIMAL_EVENT_SIZE = 22;
  size_t eventCount = data->size() / MINIMAL_EVENT_SIZE;

  // Validate that data size is a multiple of MinimalEventData size
  if (data->size() % MINIMAL_EVENT_SIZE != 0) {
    return nullptr;  // Invalid data size
  }

  auto events =
      std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
  events->reserve(eventCount);

  // Deserialize each 22-byte record
  const uint8_t *rawData = data->data();
  for (size_t i = 0; i < eventCount; ++i) {
    const uint8_t *eventData = rawData + (i * MINIMAL_EVENT_SIZE);

    // Directly cast binary data back to MinimalEventData struct
    const MinimalEventData *sourceEvent =
        reinterpret_cast<const MinimalEventData *>(eventData);

    // Create new event and copy data
    auto event = std::make_unique<MinimalEventData>(*sourceEvent);
    events->push_back(std::move(event));
  }

  return events;
}

// Sequence number management
uint64_t DataProcessor::GetNextSequence()
{
  return sequence_counter_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t DataProcessor::GetCurrentSequence() const
{
  return sequence_counter_.load(std::memory_order_relaxed);
}

void DataProcessor::ResetSequence()
{
  sequence_counter_.store(0, std::memory_order_relaxed);
}

// Auto-sequence processing
std::unique_ptr<std::vector<uint8_t>> DataProcessor::ProcessWithAutoSequence(
    const std::unique_ptr<std::vector<std::unique_ptr<EventData>>> &events)
{
  return Process(events, GetNextSequence());
}

std::unique_ptr<std::vector<uint8_t>> DataProcessor::ProcessWithAutoSequence(
    const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>
        &events)
{
  return Process(events, GetNextSequence());
}

// EOS (End Of Stream) message creation
std::unique_ptr<std::vector<uint8_t>> DataProcessor::CreateEOSMessage()
{
  // Create header-only message with EOS type
  BinaryDataHeader header{};
  header.magic_number = BINARY_DATA_MAGIC_NUMBER;
  header.sequence_number = GetNextSequence();
  header.format_version = FORMAT_VERSION_EVENTDATA;
  header.header_size = BINARY_DATA_HEADER_SIZE;
  header.event_count = 0;  // No events in EOS
  header.uncompressed_size = 0;
  header.compressed_size = 0;
  header.checksum = 0;  // No payload to checksum
  header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  header.compression_type = COMPRESSION_NONE;
  header.checksum_type = CHECKSUM_NONE;
  header.message_type = MESSAGE_TYPE_EOS;

  // Clear reserved fields
  std::memset(header.reserved, 0, sizeof(header.reserved));

  // Create output with header only (no payload)
  auto result = std::make_unique<std::vector<uint8_t>>(sizeof(header));
  std::memcpy(result->data(), &header, sizeof(header));

  return result;
}

bool DataProcessor::IsEOSMessage(const std::vector<uint8_t> &data)
{
  return IsEOSMessage(data.data(), data.size());
}

bool DataProcessor::IsEOSMessage(const uint8_t *data, size_t size)
{
  if (!data || size < sizeof(BinaryDataHeader)) {
    return false;
  }

  const BinaryDataHeader *header =
      reinterpret_cast<const BinaryDataHeader *>(data);

  // Validate magic number first
  if (header->magic_number != BINARY_DATA_MAGIC_NUMBER) {
    return false;
  }

  return header->message_type == MESSAGE_TYPE_EOS;
}

}  // namespace DELILA::Net