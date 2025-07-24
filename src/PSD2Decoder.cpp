#include "PSD2Decoder.hpp"

#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace DELILA::Digitizer::PSD2Constants;

namespace DELILA
{
namespace Digitizer
{

// Constants are now defined in PSD2Constants.hpp

// ============================================================================
// Constructor/Destructor
// ============================================================================

PSD2Decoder::PSD2Decoder(uint32_t nThreads)
{
  if (nThreads < 1) {
    nThreads = 1;
  }

  // Initialize data storage
  fEventDataVec = std::make_unique<std::vector<std::unique_ptr<EventData>>>();

  // Start decode threads
  fDecodeFlag = true;
  fDecodeThreads.reserve(nThreads);
  for (uint32_t i = 0; i < nThreads; ++i) {
    fDecodeThreads.emplace_back(&PSD2Decoder::DecodeThread, this);
  }
}

PSD2Decoder::~PSD2Decoder()
{
  // Signal threads to stop
  fDecodeFlag = false;

  // Wait for all threads to finish
  for (auto &thread : fDecodeThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

// ============================================================================
// Data Access
// ============================================================================

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
PSD2Decoder::GetEventData()
{
  auto data = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
  {
    std::lock_guard<std::mutex> lock(fEventDataMutex);
    data->swap(*fEventDataVec);
    fEventDataVec->clear();
  }
  return data;
}

// ============================================================================
// Threading and Data Processing
// ============================================================================

void PSD2Decoder::DecodeThread()
{
  while (fDecodeFlag) {
    std::unique_ptr<RawData_t> rawData = nullptr;

    // Get data from queue
    {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      if (fRawDataQueue.empty()) {
        // Release lock before sleeping to avoid deadlock
      } else {
        rawData = std::move(fRawDataQueue.front());
        fRawDataQueue.pop_front();
      }
    }
    
    // Sleep outside the lock if no data was available
    if (!rawData) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    // Process data if available
    if (rawData) {
      DecodeData(std::move(rawData));
    }
  }
}

void PSD2Decoder::DecodeData(std::unique_ptr<RawData_t> rawData)
{
  if (fDumpFlag) {
    DumpRawData(*rawData);
  }

  // Read and validate header
  uint64_t headerWord = 0;
  std::memcpy(&headerWord, rawData->data.data(), sizeof(uint64_t));

  if (!ValidateDataHeader(headerWord, rawData->size)) {
    return;
  }

  // Extract total size from header
  auto totalSize = static_cast<uint32_t>(headerWord & Header::kTotalSizeMask);

  // Process all events in the data
  ProcessEventData(rawData->data.begin(), totalSize);
}

void PSD2Decoder::DumpRawData(const RawData_t &rawData) const
{
  std::cout << "Data size: " << rawData.size << std::endl;
  for (size_t i = 0; i < rawData.size; i += kWordSize) {
    uint64_t buf = 0;
    std::memcpy(&buf, &rawData.data[i], kWordSize);
    std::cout << std::bitset<64>(buf) << std::endl;
  }
}

bool PSD2Decoder::ValidateDataHeader(uint64_t headerWord, size_t dataSize)
{
  // Check header type
  auto headerType = (headerWord >> Header::kTypeShift) & Header::kTypeMask;
  if (headerType != Header::kTypeData) {
    std::cerr << "Invalid header type: 0x" << std::hex << headerType << std::dec
              << std::endl;
    return false;
  }

  // Check fail bit
  auto failCheck = (headerWord >> Header::kFailCheckShift) & Header::kFailCheckMask;
  if (failCheck) {
    std::cerr << "Board fail bit set" << std::endl;
  }

  // Validate aggregate counter for single-threaded mode
  auto aggregateCounter =
      (headerWord >> Header::kAggregateCounterShift) & Header::kAggregateCounterMask;
  if (fDecodeThreads.size() == 1) {
    if (aggregateCounter != 0 && aggregateCounter != fLastCounter + 1) {
      std::cerr << "Aggregate counter discontinuity: " << fLastCounter << " -> "
                << aggregateCounter << std::endl;
    }
    fLastCounter = aggregateCounter;
  }

  // Validate total size
  auto totalSize = static_cast<uint32_t>(headerWord & Header::kTotalSizeMask);
  if (totalSize * kWordSize != dataSize) {
    std::cerr << "Size mismatch: header=" << totalSize * kWordSize
              << " actual=" << dataSize << std::endl;
  }

  return true;
}

void PSD2Decoder::ProcessEventData(
    const std::vector<uint8_t>::iterator &dataStart, uint32_t totalSize)
{
  // Direct EventData output
  std::vector<std::unique_ptr<EventData>> eventDataVec;
  eventDataVec.reserve(totalSize / 2);

  for (size_t wordIndex = 1; wordIndex < totalSize;) {
    auto eventData = DecodeEventPair(dataStart, wordIndex);
    if (eventData) {
      eventDataVec.push_back(std::move(eventData));
    }
  }

  // Sort EventData by timeStampNs in ascending order
  if (!eventDataVec.empty()) {
    std::sort(eventDataVec.begin(), eventDataVec.end(),
              [](const std::unique_ptr<EventData> &a,
                 const std::unique_ptr<EventData> &b) {
                return a->timeStampNs < b->timeStampNs;
              });
  }

  // Store converted data
  {
    std::lock_guard<std::mutex> lock(fEventDataMutex);
    fEventDataVec->insert(fEventDataVec->end(),
                          std::make_move_iterator(eventDataVec.begin()),
                          std::make_move_iterator(eventDataVec.end()));
  }
}

std::unique_ptr<EventData> PSD2Decoder::DecodeEventPair(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex)
{
  // Read first word (channel and timestamp)
  uint64_t firstWord = 0;
  std::memcpy(&firstWord, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;

  // Read second word (flags and energy)
  uint64_t secondWord = 0;
  std::memcpy(&secondWord, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;

  // Check for waveform data to determine size
  bool hasWaveform = (secondWord >> Event::kWaveformFlagShift) & 0x1;
  size_t waveformSize = 0;
  if (hasWaveform) {
    // Peek at waveform header to get size
    uint64_t waveformHeader = 0;
    std::memcpy(&waveformHeader, &(*(dataStart + (wordIndex + 1) * kWordSize)),
                sizeof(uint64_t));
    uint64_t nWordsWaveform = 0;
    std::memcpy(&nWordsWaveform, &(*(dataStart + (wordIndex + 2) * kWordSize)),
                sizeof(uint64_t));
    nWordsWaveform &= Waveform::kWaveformWordsMask;
    waveformSize = nWordsWaveform * 2;  // 2 points per word
  }

  // Create EventData with proper size
  auto eventData = std::make_unique<EventData>(waveformSize);

  // Extract raw timestamp from first word
  uint64_t rawTimeStamp = firstWord & Event::kTimeStampMask;

  // Decode first word
  DecodeFirstWord(firstWord, *eventData);

  // Decode second word (needs raw timestamp for time calculation)
  DecodeSecondWord(secondWord, *eventData, rawTimeStamp);

  // Check for waveform data
  if (hasWaveform) {
    DecodeWaveformData(dataStart, wordIndex, *eventData);
  }

  eventData->timeResolution = fTimeStep;
  eventData->module = fModuleNumber;

  return eventData;
}

void PSD2Decoder::DecodeFirstWord(uint64_t word, EventData &eventData) const
{
  // Extract channel
  eventData.channel = (word >> Event::kChannelShift) & Event::kChannelMask;

  // Extract raw timestamp (store in temporary variable for debugging)
  uint64_t rawTimeStamp = word & Event::kTimeStampMask;

  if (fDumpFlag) {
    std::cout << "Channel: " << static_cast<int>(eventData.channel)
              << std::endl;
    std::cout << "Time stamp (raw): " << rawTimeStamp << std::endl;
  }
}

void PSD2Decoder::DecodeSecondWord(uint64_t word, EventData &eventData,
                                   uint64_t rawTimeStamp) const
{
  // Extract flags and store in 64-bit flags field
  uint64_t flagsLowPriority =
      (word >> Event::kFlagsLowPriorityShift) & Event::kFlagsLowPriorityMask;
  uint64_t flagsHighPriority =
      (word >> Event::kFlagsHighPriorityShift) & Event::kFlagsHighPriorityMask;
  eventData.flags = (flagsHighPriority << 11) |
                    flagsLowPriority;  // Combine into single field

  // Extract energies
  eventData.energyShort = (word >> Event::kEnergyShortShift) & Event::kEnergyShortMask;
  eventData.energy = word & Event::kEnergyMask;

  // Calculate precise timestamp with fine time correction
  uint64_t fineTime = (word >> Event::kFineTimeShift) & Event::kFineTimeMask;
  double coarseTimeNs = static_cast<double>(rawTimeStamp) * fTimeStep;
  double fineTimeNs =
      (static_cast<double>(fineTime) / Event::kFineTimeScale) * fTimeStep;
  eventData.timeStampNs = coarseTimeNs + fineTimeNs;

  if (fDumpFlag) {
    std::cout << "Flags: " << eventData.flags << std::endl;
    std::cout << "Short gate: " << eventData.energyShort << std::endl;
    std::cout << "Energy: " << eventData.energy << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Final timestamp: " << eventData.timeStampNs << " ns"
              << std::endl;
    std::cout << std::defaultfloat;
  }
}

void PSD2Decoder::DecodeWaveformData(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex,
    EventData &eventData)
{
  // Read waveform header
  uint64_t waveformHeader = 0;
  std::memcpy(&waveformHeader, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;

  // Validate waveform header
  bool headerValid =
      ((waveformHeader >> Waveform::kWaveformCheck1Shift) & 0x1) == 0x1 &&
      ((waveformHeader >> Waveform::kWaveformCheck2Shift) & Waveform::kWaveformCheck2Mask) == 0x0;
  if (!headerValid) {
    std::cerr << "Invalid waveform header" << std::endl;
  }

  // Decode waveform header
  DecodeWaveformHeader(waveformHeader, eventData);

  // Get waveform configuration
  WaveformConfig config = ExtractWaveformConfig(waveformHeader);

  // Read number of waveform words
  uint64_t nWordsWaveform = 0;
  std::memcpy(&nWordsWaveform, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;
  nWordsWaveform &= Waveform::kWaveformWordsMask;

  // EventData is already properly sized, verify it matches
  size_t expectedSize = nWordsWaveform * 2;
  if (eventData.waveformSize != expectedSize) {
    std::cerr << "Waveform size mismatch: expected " << expectedSize << ", got "
              << eventData.waveformSize << std::endl;
  }

  // Decode waveform data
  for (size_t j = 0; j < nWordsWaveform; j++) {
    uint64_t waveformWord = 0;
    std::memcpy(&waveformWord, &(*(dataStart + wordIndex * kWordSize)),
                sizeof(uint64_t));
    wordIndex++;

    // Each word contains two 32-bit samples
    uint32_t sample1 = static_cast<uint32_t>(waveformWord & 0xFFFFFFFF);
    uint32_t sample2 = static_cast<uint32_t>((waveformWord >> 32) & 0xFFFFFFFF);

    DecodeWaveformPoint(sample1, j * 2, config, eventData);
    DecodeWaveformPoint(sample2, j * 2 + 1, config, eventData);
  }
}

void PSD2Decoder::DecodeWaveformHeader(uint64_t header,
                                       EventData &eventData) const
{
  // Extract time resolution
  auto timeResolution = (header >> Waveform::kTimeResolutionShift) & Waveform::kTimeResolutionMask;
  eventData.downSampleFactor = 1 << timeResolution;  // 1, 2, 4, or 8

  // Extract probe types
  eventData.digitalProbe4Type = static_cast<uint8_t>((header >> 24) & 0xF);
  eventData.digitalProbe3Type = static_cast<uint8_t>((header >> 20) & 0xF);
  eventData.digitalProbe2Type = static_cast<uint8_t>((header >> 16) & 0xF);
  eventData.digitalProbe1Type = static_cast<uint8_t>((header >> 12) & 0xF);
  eventData.analogProbe2Type = static_cast<uint8_t>((header >> 6) & 0x7);
  eventData.analogProbe1Type = static_cast<uint8_t>(header & 0x7);
}

PSD2Decoder::WaveformConfig PSD2Decoder::ExtractWaveformConfig(
    uint64_t header) const
{
  WaveformConfig config;

  // Analog probe 1 configuration
  config.ap1IsSigned = (header >> 3) & 0x1;
  config.ap1MulFactor = GetMultiplicationFactor((header >> 4) & 0x3);

  // Analog probe 2 configuration
  config.ap2IsSigned = (header >> 9) & 0x1;
  config.ap2MulFactor = GetMultiplicationFactor((header >> 10) & 0x3);

  return config;
}

uint32_t PSD2Decoder::GetMultiplicationFactor(uint32_t encodedValue) const
{
  switch (encodedValue) {
    case 0:
      return 1;
    case 1:
      return 4;
    case 2:
      return 8;
    case 3:
      return 16;
    default:
      return 1;
  }
}

void PSD2Decoder::DecodeWaveformPoint(uint32_t point, size_t dataIndex,
                                      const WaveformConfig &config,
                                      EventData &eventData) const
{
  // Decode analog probes
  uint32_t analog1Raw = point & Waveform::kAnalogProbeMask;
  uint32_t analog2Raw = (point >> 16) & Waveform::kAnalogProbeMask;

  if (config.ap1IsSigned) {
    // Sign extend from 14-bit to 32-bit
    if (analog1Raw & 0x2000) analog1Raw |= 0xFFFFC000;
    eventData.analogProbe1[dataIndex] =
        static_cast<int32_t>(analog1Raw) * config.ap1MulFactor;
  } else {
    eventData.analogProbe1[dataIndex] = analog1Raw * config.ap1MulFactor;
  }

  if (config.ap2IsSigned) {
    // Sign extend from 14-bit to 32-bit
    if (analog2Raw & 0x2000) analog2Raw |= 0xFFFFC000;
    eventData.analogProbe2[dataIndex] =
        static_cast<int32_t>(analog2Raw) * config.ap2MulFactor;
  } else {
    eventData.analogProbe2[dataIndex] = analog2Raw * config.ap2MulFactor;
  }

  // Decode digital probes
  eventData.digitalProbe1[dataIndex] = (point >> 14) & Waveform::kDigitalProbeMask;
  eventData.digitalProbe2[dataIndex] = (point >> 15) & Waveform::kDigitalProbeMask;
  eventData.digitalProbe3[dataIndex] = (point >> 30) & Waveform::kDigitalProbeMask;
  eventData.digitalProbe4[dataIndex] = (point >> 31) & Waveform::kDigitalProbeMask;
}

// ============================================================================
// Data Input and Classification
// ============================================================================

DataType PSD2Decoder::AddData(std::unique_ptr<RawData_t> rawData)
{
  constexpr uint32_t oneWordSize = kWordSize;
  if (rawData->size % oneWordSize != 0) {
    std::cerr << "Data size is not a multiple of " << oneWordSize << " Bytes"
              << std::endl;
    return DataType::Unknown;
  }

  // change big endian to little endian
  for (size_t i = 0; i < rawData->size; i += oneWordSize) {
    std::reverse(rawData->data.begin() + i,
                 rawData->data.begin() + i + oneWordSize);
  }

  auto dataType = CheckDataType(rawData);
  if (dataType == DataType::Event) {
    if (fIsRunning) {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      fRawDataQueue.push_back(std::move(rawData));
    }
  } else if (dataType == DataType::Start) {
    fIsRunning = true;
  } else if (dataType == DataType::Stop) {
    fIsRunning = false;
  } else if (dataType == DataType::Unknown) {
    std::cout << "Unknown data type" << std::endl;
    exit(1);
  }

  return dataType;
}

// ============================================================================
// Data Type Detection
// ============================================================================

DataType PSD2Decoder::CheckDataType(std::unique_ptr<RawData_t> &rawData)
{
  if (rawData->size < 3 * kWordSize) {
    return DataType::Unknown;
  } else if (rawData->size == 3 * kWordSize) {
    if (CheckStop(rawData)) {
      return DataType::Stop;
    }
  } else if (rawData->size == 4 * kWordSize) {
    if (CheckStart(rawData)) {
      return DataType::Start;
    }
  }

  return DataType::Event;
}

// ============================================================================
// Specific Data Type Checkers
// ============================================================================

bool PSD2Decoder::CheckStop(std::unique_ptr<RawData_t> &rawData)
{
  uint64_t buf = 0;
  // The first word bit[60:63] = 0x3
  // The first word bit[56:59] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto firstCondition =
      ((buf >> 60) & 0xF) == 0x3 && ((buf >> 56) & 0xF) == 0x2;

  // The second word bit[56:63] = 0x0
  std::memcpy(&buf, &(*(rawData->data.begin() + 8)), sizeof(uint64_t));
  auto secondCondition = ((buf >> 56) & 0xF) == 0x0;

  // The third word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 16)), sizeof(uint64_t));
  auto thirdCondition = ((buf >> 56) & 0xF) == 0x1;

  if (firstCondition && secondCondition && thirdCondition) {
    // The third word bit[0:31] = dead time
    // auto deadTime = static_cast<uint32_t>(buf & 0xFFFFFFFF);
    // std::cout << "Dead time: " << deadTime * 8 << " ns" << std::endl;
    return true;
  }

  return false;
}

bool PSD2Decoder::CheckStart(std::unique_ptr<RawData_t> &rawData)
{
  uint64_t buf = 0;
  // The first word bit[60:63] = 0x3
  // The first word bit[56:59] = 0x0
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto firstCondition =
      ((buf >> 60) & 0xF) == 0x3 && ((buf >> 56) & 0xF) == 0x0;

  // The second word bit[56:63] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin() + 8)), sizeof(uint64_t));
  auto secondCondition = ((buf >> 56) & 0xF) == 0x2;

  // The third word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 16)), sizeof(uint64_t));
  auto thirdCondition = ((buf >> 56) & 0xF) == 0x1;

  // The fourth word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 24)), sizeof(uint64_t));
  auto fourthCondition = ((buf >> 56) & 0xF) == 0x1;

  if (firstCondition && secondCondition && thirdCondition && fourthCondition) {
    return true;
  }

  return false;
}

}  // namespace Digitizer
}  // namespace DELILA
