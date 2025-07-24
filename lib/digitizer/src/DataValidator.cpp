#include "DataValidator.hpp"

namespace DELILA
{
namespace Digitizer
{

DecoderResult DataValidator::ValidateBoardHeader(const uint32_t *headerWords)
{
  if (headerWords == nullptr) {
    DecoderLogger::LogError("DataValidator",
                            "Board header words pointer is null");
    return DecoderResult::CorruptedData;
  }

  // Validate header type
  uint32_t headerType =
      (headerWords[0] >> PSD1Constants::BoardHeader::kTypeShift) &
      PSD1Constants::BoardHeader::kTypeMask;
  if (headerType != PSD1Constants::BoardHeader::kTypeData) {
    DecoderLogger::LogError("DataValidator", "Invalid board header type: 0x" +
                                                 std::to_string(headerType));
    return DecoderResult::InvalidHeader;
  }

  // Validate aggregate size
  uint32_t aggregateSize =
      headerWords[0] & PSD1Constants::BoardHeader::kAggregateSizeMask;
  if (aggregateSize < PSD1Constants::BoardHeader::kHeaderSizeWords) {
    DecoderLogger::LogError(
        "DataValidator",
        "Board aggregate size too small: " + std::to_string(aggregateSize));
    return DecoderResult::CorruptedData;
  }

  // Validate board ID
  uint32_t boardId =
      (headerWords[1] >> PSD1Constants::BoardHeader::kBoardIdShift) &
      PSD1Constants::BoardHeader::kBoardIdMask;
  if (boardId > PSD1Constants::Validation::kMaxBoardId) {
    DecoderLogger::LogError("DataValidator",
                            "Invalid board ID: " + std::to_string(boardId));
    return DecoderResult::CorruptedData;
  }

  // Validate dual channel mask (should have at least one active channel)
  uint32_t dualChannelMask =
      (headerWords[1] >> PSD1Constants::BoardHeader::kDualChannelMaskShift) &
      PSD1Constants::BoardHeader::kDualChannelMaskMask;
  if (dualChannelMask == 0) {
    DecoderLogger::LogWarning("DataValidator",
                              "No active channels in dual channel mask");
  }

  return DecoderResult::Success;
}

DecoderResult DataValidator::ValidateDualChannelHeader(
    const uint32_t *headerWords)
{
  if (headerWords == nullptr) {
    DecoderLogger::LogError("DataValidator",
                            "Dual channel header words pointer is null");
    return DecoderResult::CorruptedData;
  }

  // Validate header flag
  bool headerFlag = (headerWords[0] >>
                     PSD1Constants::ChannelHeader::kDualChannelHeaderShift) &
                    0x1;
  if (!headerFlag) {
    DecoderLogger::LogError("DataValidator",
                            "Invalid dual channel header flag");
    return DecoderResult::InvalidHeader;
  }

  // Validate aggregate size
  uint32_t aggregateSize =
      headerWords[0] & PSD1Constants::ChannelHeader::kDualChannelSizeMask;
  if (aggregateSize < PSD1Constants::ChannelHeader::kHeaderSizeWords) {
    DecoderLogger::LogError("DataValidator",
                            "Dual channel aggregate size too small: " +
                                std::to_string(aggregateSize));
    return DecoderResult::CorruptedData;
  }

  // Validate waveform samples
  uint32_t numSamplesWave =
      headerWords[1] & PSD1Constants::ChannelHeader::kNumSamplesWaveMask;
  size_t totalSamples =
      numSamplesWave * PSD1Constants::Waveform::kSamplesPerGroup;
  if (totalSamples > PSD1Constants::Validation::kMaxWaveformSamples) {
    DecoderLogger::LogError("DataValidator", "Waveform samples too large: " +
                                                 std::to_string(totalSamples));
    return DecoderResult::InvalidWaveformSize;
  }

  // Validate probe settings
  uint8_t digitalProbe1 =
      (headerWords[1] >> PSD1Constants::ChannelHeader::kDigitalProbe1Shift) &
      PSD1Constants::ChannelHeader::kDigitalProbe1Mask;
  uint8_t digitalProbe2 =
      (headerWords[1] >> PSD1Constants::ChannelHeader::kDigitalProbe2Shift) &
      PSD1Constants::ChannelHeader::kDigitalProbe2Mask;
  uint8_t analogProbe =
      (headerWords[1] >> PSD1Constants::ChannelHeader::kAnalogProbeShift) &
      PSD1Constants::ChannelHeader::kAnalogProbeMask;

  return ValidateProbeConfiguration(digitalProbe1, digitalProbe2, analogProbe);
}

DecoderResult DataValidator::ValidateEventData(
    uint32_t eventWord, size_t availableWords,
    const DualChannelInfo &dualChInfo)
{
  size_t requiredWords = 1;  // At least the event header word

  // Add words for enabled components
  if (dualChInfo.extrasEnabled) {
    requiredWords += 1;
  }
  if (dualChInfo.chargeEnabled) {
    requiredWords += 1;
  }
  if (dualChInfo.samplesEnabled) {
    // Calculate waveform words needed
    size_t waveformWords =
        dualChInfo.numSamplesWave * PSD1Constants::Waveform::kSamplesPerWord;
    requiredWords += waveformWords;
  }

  if (!IsSizeSufficient(availableWords, requiredWords, "Event data")) {
    return DecoderResult::InsufficientData;
  }

  // Validate trigger time tag
  uint32_t triggerTimeTag =
      eventWord & PSD1Constants::Event::kTriggerTimeTagMask;
  if (triggerTimeTag == 0) {
    DecoderLogger::LogWarning("DataValidator", "Zero trigger time tag");
  }

  return DecoderResult::Success;
}

DecoderResult DataValidator::ValidateWaveformData(size_t numSamples,
                                                  size_t availableWords)
{
  if (numSamples == 0) {
    return DecoderResult::Success;  // No waveform data is valid
  }

  if (numSamples > PSD1Constants::Validation::kMaxWaveformSamples) {
    DecoderLogger::LogError(
        "DataValidator",
        "Waveform samples exceed maximum: " + std::to_string(numSamples));
    return DecoderResult::InvalidWaveformSize;
  }

  // Calculate required words (2 samples per word)
  size_t requiredWords =
      (numSamples + 1) / PSD1Constants::Waveform::kSamplesPerWord;

  if (!IsSizeSufficient(availableWords, requiredWords, "Waveform data")) {
    return DecoderResult::InsufficientData;
  }

  return DecoderResult::Success;
}

DecoderResult DataValidator::ValidateTimestamp(uint32_t triggerTimeTag,
                                               uint16_t extendedTime,
                                               uint16_t fineTime)
{
  // Validate trigger time tag (should not be all bits set)
  if (triggerTimeTag == PSD1Constants::Event::kTriggerTimeTagMask) {
    DecoderLogger::LogWarning(
        "DataValidator",
        "Trigger time tag has all bits set (potentially invalid)");
  }

  // Validate fine time (should be within 10-bit range)
  if (fineTime > PSD1Constants::Event::kFineTimeStampMask) {
    DecoderLogger::LogError("DataValidator", "Fine time stamp out of range: " +
                                                 std::to_string(fineTime));
    return DecoderResult::TimestampError;
  }

  // Extended time validation (16-bit value, all values are theoretically valid)
  // No specific validation needed for extended time

  return DecoderResult::Success;
}

DecoderResult DataValidator::ValidateChargeData(uint32_t chargeWord)
{
  uint32_t chargeShort = chargeWord & PSD1Constants::Event::kChargeShortMask;
  uint32_t chargeLong = (chargeWord >> PSD1Constants::Event::kChargeLongShift) &
                        PSD1Constants::Event::kChargeLongMask;

  // Validate charge values (should be reasonable, not all zeros or all ones)
  if (chargeShort == 0 && chargeLong == 0) {
    DecoderLogger::LogWarning("DataValidator", "Both charge values are zero");
  }

  if (chargeShort == PSD1Constants::Event::kChargeShortMask &&
      chargeLong == PSD1Constants::Event::kChargeLongMask) {
    DecoderLogger::LogWarning("DataValidator",
                              "Both charge values are at maximum");
  }

  return DecoderResult::Success;
}

DecoderResult DataValidator::ValidateBlockBounds(size_t blockStart,
                                                 size_t blockEnd,
                                                 size_t totalSize,
                                                 const std::string &blockName)
{
  if (blockStart > blockEnd) {
    DecoderLogger::LogError(
        "DataValidator",
        blockName + " block start > end: " + std::to_string(blockStart) +
            " > " + std::to_string(blockEnd));
    return DecoderResult::CorruptedData;
  }

  if (blockEnd > totalSize) {
    DecoderLogger::LogError(
        "DataValidator",
        blockName + " block extends beyond data: " + std::to_string(blockEnd) +
            " > " + std::to_string(totalSize));
    return DecoderResult::OutOfBounds;
  }

  return DecoderResult::Success;
}

DecoderResult DataValidator::ValidateProbeConfiguration(uint8_t digitalProbe1,
                                                        uint8_t digitalProbe2,
                                                        uint8_t analogProbe)
{
  // Validate digital probe 1
  if (!IsInRange(digitalProbe1, 0, 7, "Digital probe 1")) {
    return DecoderResult::CorruptedData;
  }

  // Validate digital probe 2
  if (!IsInRange(digitalProbe2, 0, 7, "Digital probe 2")) {
    return DecoderResult::CorruptedData;
  }

  // Validate analog probe
  if (!IsInRange(analogProbe, 0, 3, "Analog probe")) {
    return DecoderResult::CorruptedData;
  }

  return DecoderResult::Success;
}

}  // namespace Digitizer
}  // namespace DELILA