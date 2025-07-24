#include "PHA1Decoder.hpp"

#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Use PHA1Constants for all constants
// ============================================================================
// Note: We'll use PHA1Constants:: prefix to avoid namespace conflicts
using PHA1Constants::kWordSize;

// ============================================================================
// Constructor/Destructor
// ============================================================================

PHA1Decoder::PHA1Decoder(uint32_t nThreads)
{
  if (nThreads < 1) {
    nThreads = 1;
  }

  // Initialize data storage
  fEventDataVec = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
  fEventDataCache = std::make_unique<std::vector<std::unique_ptr<EventData>>>();

  // Pre-allocate with reasonable defaults
  fEventDataVec->reserve(fEventDataCacheSize);
  fEventDataCache->reserve(fEventDataCacheSize);

  // Update cached values
  UpdateCachedValues();

  // Start in running state (dig1 doesn't have explicit start/stop like dig2)
  fIsRunning = true;

  // Start decode threads
  fDecodeFlag = true;
  fDecodeThreads.reserve(nThreads);
  for (uint32_t i = 0; i < nThreads; ++i) {
    fDecodeThreads.emplace_back(&PHA1Decoder::DecodeThread, this);
  }
}

PHA1Decoder::~PHA1Decoder()
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
PHA1Decoder::GetEventData()
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

void PHA1Decoder::DecodeThread()
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

void PHA1Decoder::DecodeData(std::unique_ptr<RawData_t> rawData)
{
  if (fDumpFlag) {
    DumpRawData(*rawData);
  }

  // Validate raw data
  DecoderResult result =
      DataValidator::ValidateRawData(rawData->data.data(), rawData->size);
  if (result != DecoderResult::Success) {
    DecoderLogger::LogResult(result, "DecodeData",
                             "Raw data validation failed");
    return;
  }

  uint32_t firstWord = 0;
  std::memcpy(&firstWord, rawData->data.data(), sizeof(uint32_t));

  DecoderResult headerResult = ValidateDataHeader(firstWord, rawData->size);
  if (headerResult != DecoderResult::Success) {
    DecoderLogger::LogResult(headerResult, "DecodeData",
                             "Header validation failed");
    return;
  }

  // Process all events in the data (multiple board aggregate blocks)
  uint32_t totalDataSizeWords = rawData->size / kWordSize;
  DecoderResult processResult =
      ProcessEventData(rawData->data.begin(), totalDataSizeWords);

  if (processResult != DecoderResult::Success) {
    DecoderLogger::LogResult(processResult, "DecodeData",
                             "Event processing failed");
  }
}

void PHA1Decoder::DumpRawData(const RawData_t &rawData) const
{
  std::cout << "PHA1 Data size: " << rawData.size << std::endl;
  for (size_t i = 0; i < rawData.size; i += kWordSize) {
    uint32_t buf = 0;
    std::memcpy(&buf, &rawData.data[i], kWordSize);
    std::cout << std::bitset<32>(buf) << std::endl;
  }
}

DecoderResult PHA1Decoder::ValidateDataHeader(uint32_t headerWord,
                                              size_t dataSize)
{
  // Check header type (should be 0b1010 for PHA1)
  auto headerType = (headerWord >> PHA1Constants::BoardHeader::kTypeShift) &
                    PHA1Constants::BoardHeader::kTypeMask;
  if (headerType != PHA1Constants::BoardHeader::kTypeData) {
    DecoderLogger::LogError(
        "ValidateDataHeader",
        "Invalid PHA1 header type: 0x" + std::to_string(headerType) +
            " (expected 0x" +
            std::to_string(PHA1Constants::BoardHeader::kTypeData) + ")");
    return DecoderResult::InvalidHeader;
  }

  // Note: totalSize is the size of this board aggregate block, not the entire data
  auto totalSize = headerWord & PHA1Constants::BoardHeader::kAggregateSizeMask;

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "ValidateDataHeader",
        "Board aggregate size: " + std::to_string(totalSize * kWordSize) +
            " bytes");
  }

  return DecoderResult::Success;
}

DecoderResult PHA1Decoder::ProcessEventData(
    const std::vector<uint8_t>::iterator &dataStart, uint32_t totalDataSize)
{
  // Direct EventData output with optimized pre-allocation
  std::vector<std::unique_ptr<EventData>> eventDataVec;
  // Better estimation: assume average event size is 20 words
  size_t estimatedEvents =
      std::max(totalDataSize / 20, static_cast<uint32_t>(1));
  eventDataVec.reserve(estimatedEvents);

  MemoryReader reader(dataStart, totalDataSize);
  size_t wordIndex = 0;

  // Process multiple Board Aggregate Blocks in the data
  while (wordIndex < totalDataSize) {
    DecoderResult result =
        ProcessBoardAggregateBlock(reader, wordIndex, eventDataVec);
    if (result != DecoderResult::Success) {
      DecoderLogger::LogResult(
          result, "ProcessEventData",
          "Failed to process board aggregate block at word " +
              std::to_string(wordIndex));
      // Continue to next block if possible
      if (result == DecoderResult::CorruptedData) {
        break;  // Stop processing on corrupted data
      }
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

  if (fDumpFlag) {
    DecoderLogger::LogDebug("ProcessEventData",
                            "Decoded " + std::to_string(eventDataVec.size()) +
                                " events from " +
                                std::to_string(totalDataSize) + " words");
  }

  // Store converted data
  {
    std::lock_guard<std::mutex> lock(fEventDataMutex);
    fEventDataVec->insert(fEventDataVec->end(),
                          std::make_move_iterator(eventDataVec.begin()),
                          std::make_move_iterator(eventDataVec.end()));
  }

  return DecoderResult::Success;
}

DecoderResult PHA1Decoder::ProcessBoardAggregateBlock(
    MemoryReader &reader, size_t &wordIndex,
    std::vector<std::unique_ptr<EventData>> &eventDataVec)
{
  // Decode board header (4 words)
  PHA1BoardHeaderInfo boardInfo;
  DecoderResult result = DecodeBoardHeader(reader, wordIndex, boardInfo);
  if (result != DecoderResult::Success) {
    return result;
  }

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "ProcessBoardAggregateBlock",
        "Processing Board Aggregate Block: size=" +
            std::to_string(boardInfo.aggregateSize) + " words, mask=0x" +
            std::to_string(static_cast<int>(boardInfo.dualChannelMask)));
  }

  // Calculate the end of this board aggregate block
  size_t boardEndIndex = wordIndex -
                         PHA1Constants::BoardHeader::kHeaderSizeWords +
                         boardInfo.aggregateSize;

  // Validate block bounds
  result =
      ValidateBlockBounds(wordIndex, boardEndIndex, reader.GetTotalSizeWords());
  if (result != DecoderResult::Success) {
    return result;
  }

  // Process channel pairs in this board aggregate
  return ProcessChannelPairs(reader, wordIndex, boardInfo, boardEndIndex,
                             eventDataVec);
}

DecoderResult PHA1Decoder::ProcessChannelPairs(
    MemoryReader &reader, size_t &wordIndex, const PHA1BoardHeaderInfo &boardInfo,
    size_t boardEndIndex, std::vector<std::unique_ptr<EventData>> &eventDataVec)
{
  uint8_t activeMask = boardInfo.dualChannelMask;

  for (int pair = 0; pair < PHA1Constants::Validation::kMaxChannelPairs;
       ++pair) {
    if (!(activeMask & (1 << pair))) {
      continue;  // This channel pair is not active
    }

    if (wordIndex >= boardEndIndex) {
      DecoderLogger::LogError("ProcessChannelPairs",
                              "Unexpected end of board aggregate block");
      return DecoderResult::OutOfBounds;
    }

    // Decode dual channel header (2 words)
    PHA1DualChannelInfo dualChInfo;
    DecoderResult result =
        DecodeDualChannelHeader(reader, wordIndex, dualChInfo);
    if (result != DecoderResult::Success) {
      DecoderLogger::LogError("ProcessChannelPairs",
                              "Failed to decode dual channel header for pair " +
                                  std::to_string(pair));
      return result;
    }

    // Calculate channel end index
    size_t channelEndIndex = wordIndex -
                             PHA1Constants::ChannelHeader::kHeaderSizeWords +
                             dualChInfo.aggregateSize;

    // Validate channel bounds
    result = ValidateBlockBounds(channelEndIndex, boardEndIndex,
                                 reader.GetTotalSizeWords());
    if (result != DecoderResult::Success) {
      DecoderLogger::LogError("ProcessChannelPairs",
                              "Channel aggregate block extends beyond board");
      channelEndIndex = boardEndIndex;  // Clamp to board end
    }

    if (fDumpFlag) {
      DecoderLogger::LogDebug(
          "ProcessChannelPairs",
          "Processing Channel Pair " + std::to_string(pair) +
              ": size=" + std::to_string(dualChInfo.aggregateSize) +
              " words, samples=" + std::to_string(dualChInfo.numSamplesWave));
    }

    // Decode events in this dual channel block
    while (wordIndex < channelEndIndex && wordIndex < boardEndIndex) {
      auto eventData = DecodeEventDirect(reader, wordIndex, dualChInfo);
      if (eventData) {
        // Set the channel pair offset
        eventData->channel += pair * 2;  // Each pair handles 2 channels
        eventDataVec.push_back(std::move(eventData));
      }
    }
  }

  return DecoderResult::Success;
}

DecoderResult PHA1Decoder::ValidateBlockBounds(size_t currentIndex,
                                               size_t endIndex,
                                               size_t totalSize)
{
  if (endIndex > totalSize) {
    DecoderLogger::LogError(
        "ValidateBlockBounds",
        "Block extends beyond data: " + std::to_string(endIndex) + " > " +
            std::to_string(totalSize));
    return DecoderResult::OutOfBounds;
  }

  if (currentIndex > endIndex) {
    DecoderLogger::LogError(
        "ValidateBlockBounds",
        "Invalid block bounds: current=" + std::to_string(currentIndex) +
            " > end=" + std::to_string(endIndex));
    return DecoderResult::CorruptedData;
  }

  return DecoderResult::Success;
}

// ============================================================================
// PHA1 Format Decoding Methods
// ============================================================================

DecoderResult PHA1Decoder::DecodeBoardHeader(MemoryReader &reader,
                                             size_t &wordIndex,
                                             PHA1BoardHeaderInfo &boardInfo)
{
  // Ensure we have enough data for the board header
  if (reader.GetRemainingWords(wordIndex) <
      PHA1Constants::BoardHeader::kHeaderSizeWords) {
    DecoderLogger::LogError("DecodeBoardHeader",
                            "Insufficient data for board header");
    return DecoderResult::InsufficientData;
  }

  // First word: aggregate size and header type
  // Read all header words
  uint32_t headerWords[4];
  for (int i = 0; i < 4; ++i) {
    headerWords[i] = reader.ReadWord32(wordIndex++);
  }

  // Validate header structure
  DecoderResult result = DataValidator::ValidateBoardHeader(headerWords);
  if (result != DecoderResult::Success) {
    return result;
  }

  // Extract board information
  boardInfo.aggregateSize =
      headerWords[0] & PHA1Constants::BoardHeader::kAggregateSizeMask;
  boardInfo.dualChannelMask =
      (headerWords[1] >> PHA1Constants::BoardHeader::kDualChannelMaskShift) &
      PHA1Constants::BoardHeader::kDualChannelMaskMask;
  boardInfo.lvdsPattern =
      (headerWords[1] >> PHA1Constants::BoardHeader::kLVDSPatternShift) &
      PHA1Constants::BoardHeader::kLVDSPatternMask;
  boardInfo.boardFailFlag =
      (headerWords[1] >> PHA1Constants::BoardHeader::kBoardFailShift) &
      PHA1Constants::BoardHeader::kBoardFailMask;
  boardInfo.boardId =
      (headerWords[1] >> PHA1Constants::BoardHeader::kBoardIdShift) &
      PHA1Constants::BoardHeader::kBoardIdMask;
  boardInfo.aggregateCounter =
      headerWords[2] & PHA1Constants::BoardHeader::kBoardCounterMask;
  boardInfo.boardTimeTag = headerWords[3];

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "DecodeBoardHeader",
        "Board Header - Aggregate Size: " +
            std::to_string(boardInfo.aggregateSize) +
            ", Dual Channel Mask: 0x" +
            std::to_string(static_cast<int>(boardInfo.dualChannelMask)) +
            ", Board ID: " +
            std::to_string(static_cast<int>(boardInfo.boardId)));
  }

  return DecoderResult::Success;
}

DecoderResult PHA1Decoder::DecodeDualChannelHeader(MemoryReader &reader,
                                                   size_t &wordIndex,
                                                   PHA1DualChannelInfo &dualChInfo)
{
  // Ensure we have enough data for the dual channel header
  if (reader.GetRemainingWords(wordIndex) <
      PHA1Constants::ChannelHeader::kHeaderSizeWords) {
    DecoderLogger::LogError("DecodeDualChannelHeader",
                            "Insufficient data for dual channel header");
    return DecoderResult::InsufficientData;
  }

  // Read header words
  uint32_t headerWords[2];
  for (int i = 0; i < 2; ++i) {
    headerWords[i] = reader.ReadWord32(wordIndex++);
  }

  // Validate header structure
  DecoderResult result = DataValidator::ValidateDualChannelHeader(headerWords);
  if (result != DecoderResult::Success) {
    return result;
  }

  // Extract dual channel information
  dualChInfo.aggregateSize =
      headerWords[0] & PHA1Constants::ChannelHeader::kDualChannelSizeMask;
  dualChInfo.numSamplesWave =
      headerWords[1] & PHA1Constants::ChannelHeader::kNumSamplesWaveMask;
  dualChInfo.digitalProbe =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kDigitalProbeShift) &
      PHA1Constants::ChannelHeader::kDigitalProbeMask;
  dualChInfo.analogProbe2 =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kAnalogProbe2Shift) &
      PHA1Constants::ChannelHeader::kAnalogProbe2Mask;
  dualChInfo.analogProbe1 =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kAnalogProbe1Shift) &
      PHA1Constants::ChannelHeader::kAnalogProbe1Mask;
  dualChInfo.extraOption =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kExtraOptionShift) &
      PHA1Constants::ChannelHeader::kExtraOptionMask;
  dualChInfo.samplesEnabled =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kSamplesEnabledShift) &
      0x1;
  dualChInfo.extras2Enabled =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kExtras2EnabledShift) &
      0x1;
  dualChInfo.timeEnabled =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kTimeEnabledShift) & 0x1;
  dualChInfo.energyEnabled =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kEnergyEnabledShift) &
      0x1;
  dualChInfo.dualTraceEnabled =
      (headerWords[1] >> PHA1Constants::ChannelHeader::kDualTraceShift) & 0x1;

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "DecodeDualChannelHeader",
        "Dual Channel Header - Aggregate Size: " +
            std::to_string(dualChInfo.aggregateSize) +
            ", Samples/8: " + std::to_string(dualChInfo.numSamplesWave) +
            ", Samples Enabled: " + std::to_string(dualChInfo.samplesEnabled));
  }

  return DecoderResult::Success;
}

std::unique_ptr<EventData> PHA1Decoder::DecodeEventDirect(
    MemoryReader &reader, size_t &wordIndex, const PHA1DualChannelInfo &dualChInfo)
{
  // Decode event header (time tag and channel flag)
  uint32_t triggerTimeTag;
  bool isOddChannel;
  DecoderResult result =
      DecodeEventHeader(reader, wordIndex, triggerTimeTag, isOddChannel);
  if (result != DecoderResult::Success) {
    DecoderLogger::LogResult(result, "DecodeEventDirect",
                             "Failed to decode event header");
    return nullptr;
  }

  // Calculate waveform size
  size_t waveformSize =
      dualChInfo.numSamplesWave * PHA1Constants::Waveform::kSamplesPerGroup;

  // Create EventData with proper size
  auto eventData = std::make_unique<EventData>(waveformSize);

  // Set basic event information
  eventData->channel =
      isOddChannel ? 1 : 0;  // Will be adjusted by caller for channel pair
  eventData->module = fModuleNumber;
  eventData->timeResolution = fTimeStep;

  // Store probe type information
  eventData->digitalProbe1Type = dualChInfo.digitalProbe;
  eventData->digitalProbe2Type = 0;  // PHA1 has only one digital probe
  eventData->analogProbe1Type = dualChInfo.analogProbe1;
  eventData->analogProbe2Type = dualChInfo.analogProbe2;

  // Decode timestamp
  result = DecodeEventTimestamp(reader, wordIndex, dualChInfo, triggerTimeTag,
                                *eventData);
  if (result != DecoderResult::Success) {
    DecoderLogger::LogResult(result, "DecodeEventDirect",
                             "Failed to decode timestamp");
    return nullptr;
  }

  // Decode other event data components
  result = DecodeEventDataComponents(reader, wordIndex, dualChInfo, *eventData);
  if (result != DecoderResult::Success) {
    DecoderLogger::LogResult(result, "DecodeEventDirect",
                             "Failed to decode event data components");
    return nullptr;
  }

  return eventData;
}

DecoderResult PHA1Decoder::DecodeEventHeader(MemoryReader &reader,
                                             size_t &wordIndex,
                                             uint32_t &triggerTimeTag,
                                             bool &isOddChannel)
{
  if (!reader.IsValidIndex(wordIndex)) {
    return DecoderResult::InsufficientData;
  }

  // Read trigger time tag (first word of event)
  uint32_t timeTagWord = reader.ReadWord32(wordIndex++);
  triggerTimeTag = timeTagWord & PHA1Constants::Event::kTriggerTimeTagMask;
  isOddChannel = (timeTagWord >> PHA1Constants::Event::kChannelFlagShift) & 0x1;

  return DecoderResult::Success;
}

DecoderResult PHA1Decoder::DecodeEventTimestamp(
    MemoryReader &reader, size_t &wordIndex, const PHA1DualChannelInfo &dualChInfo,
    uint32_t triggerTimeTag, EventData &eventData)
{
  if (dualChInfo.extras2Enabled) {
    if (!reader.IsValidIndex(wordIndex)) {
      return DecoderResult::InsufficientData;
    }

    uint32_t extrasWord = reader.ReadWord32(wordIndex++);

    // Extract extended timestamp and fine timestamp based on extra option
    uint16_t extendedTime = 0;
    uint16_t fineTimeStamp = 0;

    DecodeExtrasWord(extrasWord, dualChInfo.extraOption, eventData,
                     extendedTime, fineTimeStamp);

    // Calculate final timestamp using extended timestamp (47-bit total)
    uint64_t extendedTimestamp = static_cast<uint64_t>(extendedTime) << 31;
    uint64_t combinedTimeTag =
        static_cast<uint64_t>(triggerTimeTag) + extendedTimestamp;

    // Apply sample time multiplication
    uint64_t finalTimestamp = combinedTimeTag * fTimeStep;

    // Add fine time correction only for option 010 (2)
    double fineTimeNs = 0.0;
    if (dualChInfo.extraOption ==
        PHA1Constants::ExtraFormats::kExtendedFlagsFineTT) {
      fineTimeNs = static_cast<double>(fineTimeStamp) * fFineTimeMultiplier;
    }

    eventData.timeStampNs = static_cast<double>(finalTimestamp) + fineTimeNs;

    if (fDumpFlag) {
      DecoderLogger::LogDebug(
          "DecodeEventTimestamp",
          "Timestamp calc: trigger=" + std::to_string(triggerTimeTag) +
              ", extended=" + std::to_string(extendedTime) +
              ", combined=" + std::to_string(combinedTimeTag) +
              ", fine=" + std::to_string(fineTimeStamp) +
              ", final=" + std::to_string(eventData.timeStampNs) + " ns" +
              ", extraOption=" + std::to_string(dualChInfo.extraOption));
    }
  } else {
    // No extended timestamp available - use only trigger time tag
    eventData.timeStampNs = static_cast<double>(triggerTimeTag) * fTimeStep;
  }

  return DecoderResult::Success;
}

DecoderResult PHA1Decoder::DecodeEventDataComponents(
    MemoryReader &reader, size_t &wordIndex, const PHA1DualChannelInfo &dualChInfo,
    EventData &eventData)
{
  // Decode waveform data if present
  if (dualChInfo.samplesEnabled && eventData.waveformSize > 0) {
    DecodeWaveform(reader, wordIndex, dualChInfo, eventData);
  }

  // Decode energy word if present
  if (dualChInfo.energyEnabled) {
    if (!reader.IsValidIndex(wordIndex)) {
      return DecoderResult::InsufficientData;
    }

    uint32_t energyWord = reader.ReadWord32(wordIndex++);
    DecodeEnergyWord(energyWord, eventData);
  }

  return DecoderResult::Success;
}

void PHA1Decoder::DecodeWaveform(MemoryReader &reader, size_t &wordIndex,
                                 const PHA1DualChannelInfo &dualChInfo,
                                 EventData &eventData)
{
  // Calculate number of words needed for waveform
  // numSamplesWave is samples/8, 2 samples per word, so *2 words total
  size_t numWords =
      dualChInfo.numSamplesWave * PHA1Constants::Waveform::kSamplesPerWord;

  // Check if we have enough data
  if (reader.GetRemainingWords(wordIndex) < numWords) {
    DecoderLogger::LogError("DecodeWaveform",
                            "Insufficient data for waveform: need " +
                                std::to_string(numWords) + " words");
    return;
  }

  for (size_t i = 0; i < numWords; ++i) {
    uint32_t waveformWord = reader.ReadWord32(wordIndex++);

    // Each word contains two 16-bit samples
    uint16_t sample1 = waveformWord & PHA1Constants::Waveform::kSampleMask;
    uint16_t sample2 =
        (waveformWord >> PHA1Constants::Waveform::kSecondSampleShift) &
        PHA1Constants::Waveform::kSampleMask;

    size_t sampleIndex1 = i * PHA1Constants::Waveform::kSamplesPerWord;
    size_t sampleIndex2 = i * PHA1Constants::Waveform::kSamplesPerWord + 1;

    if (sampleIndex1 < eventData.waveformSize) {
      // Extract analog data (14 bits)
      eventData.analogProbe1[sampleIndex1] =
          sample1 & PHA1Constants::Waveform::kAnalogSampleMask;

      // Extract digital probes
      eventData.digitalProbe1[sampleIndex1] =
          (sample1 >> PHA1Constants::Waveform::kDigitalProbeWaveShift) & 0x1;
      eventData.digitalProbe2[sampleIndex1] =
          (sample1 >> PHA1Constants::Waveform::kTriggerFlagShift) & 0x1;

      // For dual trace mode, second analog probe data comes from odd samples
      if (dualChInfo.dualTraceEnabled && sampleIndex1 > 0) {
        eventData.analogProbe2[sampleIndex1] =
            eventData.analogProbe1[sampleIndex1 - 1];
      }
    }

    if (sampleIndex2 < eventData.waveformSize) {
      // Extract analog data (14 bits)
      eventData.analogProbe1[sampleIndex2] =
          sample2 & PHA1Constants::Waveform::kAnalogSampleMask;

      // Extract digital probes
      eventData.digitalProbe1[sampleIndex2] =
          (sample2 >> PHA1Constants::Waveform::kDigitalProbeWaveShift) & 0x1;
      eventData.digitalProbe2[sampleIndex2] =
          (sample2 >> PHA1Constants::Waveform::kTriggerFlagShift) & 0x1;

      // For dual trace mode, odd samples contain second analog probe data
      if (dualChInfo.dualTraceEnabled) {
        eventData.analogProbe2[sampleIndex2] =
            eventData.analogProbe1[sampleIndex2];
        // Restore the correct first analog probe data (from even sample)
        eventData.analogProbe1[sampleIndex2] =
            eventData.analogProbe1[sampleIndex2 - 1];
      }
    }
  }
}

void PHA1Decoder::DecodeExtrasWord(uint32_t extrasWord, uint8_t extraOption,
                                   ::DELILA::Digitizer::EventData &eventData,
                                   uint16_t &extendedTime,
                                   uint16_t &fineTimeStamp)
{
  // Initialize outputs
  extendedTime = 0;
  fineTimeStamp = 0;
  eventData.flags = 0;

  // Decode based on extra option format
  switch (extraOption) {
    case PHA1Constants::ExtraFormats::
        kExtendedTimestampOnly:  // 0b000 - Extended timestamp only
    case PHA1Constants::ExtraFormats::
        kExtendedTimestampOnly1:  // 0b001 - Extended timestamp only
      extendedTime = (extrasWord >> PHA1Constants::Event::kExtendedTimeShift) &
                     PHA1Constants::Event::kExtendedTimeMask;

      if (fDumpFlag) {
        DecoderLogger::LogDebug(
            "DecodeExtrasWord",
            "Extra option " + std::to_string(extraOption) +
                " - Extended Time: " + std::to_string(extendedTime));
      }
      break;

    case PHA1Constants::ExtraFormats::
        kExtendedFlagsFineTT:  // 0b010 - Extended timestamp + flags + fine timestamp
    {
      fineTimeStamp = extrasWord & PHA1Constants::Event::kFineTimeStampMask;
      uint8_t flags = (extrasWord >> PHA1Constants::Event::kFlagsShift) &
                      PHA1Constants::Event::kFlagsMask;
      extendedTime = (extrasWord >> PHA1Constants::Event::kExtendedTimeShift) &
                     PHA1Constants::Event::kExtendedTimeMask;

      // Store flags in EventData.flags field
      if (flags & 0x20)
        eventData.flags |= ::DELILA::Digitizer::EventData::
            FLAG_TRIGGER_LOST;  // bit[15] = bit[5] in 6-bit flags
      if (flags & 0x10)
        eventData.flags |= ::DELILA::Digitizer::EventData::
            FLAG_OVER_RANGE;  // bit[14] = bit[4] in 6-bit flags
      if (flags & 0x08)
        eventData.flags |= ::DELILA::Digitizer::EventData::
            FLAG_1024_TRIGGER;  // bit[13] = bit[3] in 6-bit flags
      if (flags & 0x04)
        eventData.flags |= ::DELILA::Digitizer::EventData::
            FLAG_N_LOST_TRIGGER;  // bit[12] = bit[2] in 6-bit flags

      if (fDumpFlag) {
        DecoderLogger::LogDebug(
            "DecodeExtrasWord",
            "Extra option 2 - Fine Time: " + std::to_string(fineTimeStamp) +
                ", Flags: 0x" + std::to_string(static_cast<int>(flags)) +
                ", Extended Time: " + std::to_string(extendedTime));
      }
    } break;

    default:
      // For other options, assume extended timestamp format
      extendedTime = (extrasWord >> PHA1Constants::Event::kExtendedTimeShift) &
                     PHA1Constants::Event::kExtendedTimeMask;

      if (fDumpFlag) {
        DecoderLogger::LogWarning(
            "DecodeExtrasWord",
            "Unknown extra option " + std::to_string(extraOption) +
                ", treating as extended timestamp only. Value: " +
                std::to_string(extendedTime));
      }
      break;
  }
}

void PHA1Decoder::DecodeEnergyWord(uint32_t energyWord,
                                   ::DELILA::Digitizer::EventData &eventData)
{
  // Extract energy value (PHA-specific format: bit[0:14])
  eventData.energy = energyWord & PHA1Constants::Event::kEnergyMask;
  
  // Extract pileup flag (bit[15])
  bool pileupFlag = (energyWord >> PHA1Constants::Event::kPileupFlagShift) & 0x1;
  
  // Extract extra data (bit[16:25])
  uint16_t extraData = (energyWord >> PHA1Constants::Event::kExtraShift) & 
                       PHA1Constants::Event::kExtraMask;

  // Store pileup flag
  if (pileupFlag) {
    eventData.flags |= ::DELILA::Digitizer::EventData::FLAG_PILEUP;
  }
  
  // Store extra data in energyShort field (reuse existing field)
  eventData.energyShort = extraData;

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "DecodeEnergyWord",
        "Energy: " + std::to_string(eventData.energy) +
            ", Pileup: " + std::to_string(pileupFlag) +
            ", Extra: " + std::to_string(extraData));
  }
}

// ============================================================================
// Data Input and Classification
// ============================================================================

DataType PHA1Decoder::AddData(std::unique_ptr<RawData_t> rawData)
{
  if (rawData->size % kWordSize != 0) {
    DecoderLogger::LogError("AddData", "PHA1 data size is not a multiple of " +
                                           std::to_string(kWordSize) +
                                           " bytes");
    return DataType::Unknown;
  }

  // PHA1 uses little endian, no byte swapping needed for most systems
  auto dataType = CheckDataType(rawData);

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "AddData", "PHA1 AddData: size=" + std::to_string(rawData->size) +
                       ", type=" + std::to_string(static_cast<int>(dataType)));
  }

  if (dataType == DataType::Event) {
    if (fIsRunning) {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      fRawDataQueue.push_back(std::move(rawData));
      if (fDumpFlag) {
        DecoderLogger::LogDebug("AddData",
                                "Added PHA1 event data to queue, queue size: " +
                                    std::to_string(fRawDataQueue.size()));
      }
    } else {
      if (fDumpFlag) {
        DecoderLogger::LogDebug(
            "AddData", "PHA1 decoder not running, discarding event data");
      }
    }
  } else if (dataType == DataType::Start) {
    fIsRunning = true;
    if (fDumpFlag) {
      DecoderLogger::LogDebug("AddData", "PHA1 decoder started");
    }
  } else if (dataType == DataType::Stop) {
    fIsRunning = false;
    if (fDumpFlag) {
      DecoderLogger::LogDebug("AddData", "PHA1 decoder stopped");
    }
  } else if (dataType == DataType::Unknown) {
    if (fDumpFlag) {
      DecoderLogger::LogDebug("AddData", "Unknown PHA1 data type, discarding");
    }
    return DataType::Unknown;
  }

  return dataType;
}

// ============================================================================
// Data Type Detection (Simplified for PHA1)
// ============================================================================

DataType PHA1Decoder::CheckDataType(std::unique_ptr<RawData_t> &rawData)
{
  if (rawData->size <
      PHA1Constants::BoardHeader::kHeaderSizeWords * kWordSize) {
    if (fDumpFlag) {
      DecoderLogger::LogDebug(
          "CheckDataType",
          "PHA1 data too small: " + std::to_string(rawData->size) + " bytes");
    }
    return DataType::Unknown;
  }

  // For PHA1, we primarily check the first word header type
  uint32_t firstWord = 0;
  std::memcpy(&firstWord, rawData->data.data(), sizeof(uint32_t));

  auto headerType = (firstWord >> PHA1Constants::BoardHeader::kTypeShift) &
                    PHA1Constants::BoardHeader::kTypeMask;

  if (fDumpFlag) {
    DecoderLogger::LogDebug(
        "CheckDataType", "PHA1 first word: 0x" + std::to_string(firstWord) +
                             ", header type: 0x" + std::to_string(headerType));
  }

  if (headerType == PHA1Constants::BoardHeader::kTypeData) {
    return DataType::Event;
  }

  // For now, treat any data as potentially valid events
  // This is more permissive than the strict header check
  if (rawData->size >= PHA1Constants::Validation::kMinimumEventSize) {
    if (fDumpFlag) {
      DecoderLogger::LogDebug("CheckDataType",
                              "Treating as Event despite header type mismatch");
    }
    return DataType::Event;
  }

  if (fDumpFlag) {
    DecoderLogger::LogDebug("CheckDataType", "Unknown data type for PHA1");
  }
  return DataType::Unknown;
}

bool PHA1Decoder::CheckStop(std::unique_ptr<RawData_t> &rawData)
{
  // TODO: Implement PHA1-specific stop detection if needed
  return false;
}

bool PHA1Decoder::CheckStart(std::unique_ptr<RawData_t> &rawData)
{
  // TODO: Implement PHA1-specific start detection if needed
  return false;
}

// ============================================================================
// Performance Optimization Methods
// ============================================================================

void PHA1Decoder::PreAllocateEventData(size_t expectedEvents)
{
  std::lock_guard<std::mutex> lock(fEventDataMutex);
  if (expectedEvents > fEventDataVec->capacity()) {
    fEventDataVec->reserve(expectedEvents);
  }
}

void PHA1Decoder::UpdateCachedValues()
{
  fFineTimeMultiplier = static_cast<double>(fTimeStep) / 1024.0;
}

}  // namespace Digitizer
}  // namespace DELILA