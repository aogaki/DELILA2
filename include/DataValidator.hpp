#ifndef DATAVALIDATOR_HPP
#define DATAVALIDATOR_HPP

#include <cstdint>
#include <string>

#include "DecoderLogger.hpp"
#include "PSD1Constants.hpp"
#include "PSD1Structures.hpp"

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Comprehensive data validation for PSD1 format
 *
 * Provides validation methods for all aspects of PSD1 data format
 * including headers, bounds checking, and data consistency.
 */
class DataValidator
{
 public:
  /**
   * @brief Validate raw data basic requirements
   * @param data Pointer to raw data
   * @param size Size of data in bytes
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateRawData(const uint8_t *data, size_t size);

  /**
   * @brief Validate board header structure
   * @param headerWords Array of 4 header words
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateBoardHeader(const uint32_t *headerWords);

  /**
   * @brief Validate dual channel header structure
   * @param headerWords Array of 2 header words
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateDualChannelHeader(const uint32_t *headerWords);

  /**
   * @brief Validate event data structure
   * @param eventWord Event header word
   * @param availableWords Number of words available for this event
   * @param dualChInfo Dual channel information
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateEventData(uint32_t eventWord,
                                         size_t availableWords,
                                         const DualChannelInfo &dualChInfo);

  /**
   * @brief Validate waveform parameters
   * @param numSamples Number of samples in waveform
   * @param availableWords Number of words available for waveform
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateWaveformData(size_t numSamples,
                                            size_t availableWords);

  /**
   * @brief Validate timestamp components
   * @param triggerTimeTag Trigger time tag
   * @param extendedTime Extended time (if present)
   * @param fineTime Fine time (if present)
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateTimestamp(uint32_t triggerTimeTag,
                                         uint16_t extendedTime = 0,
                                         uint16_t fineTime = 0);

  /**
   * @brief Validate charge data
   * @param chargeWord Charge word
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateChargeData(uint32_t chargeWord);

  /**
   * @brief Validate block bounds
   * @param blockStart Start index of block
   * @param blockEnd End index of block
   * @param totalSize Total available size
   * @param blockName Name of block for error reporting
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateBlockBounds(size_t blockStart, size_t blockEnd,
                                           size_t totalSize,
                                           const std::string &blockName);

  /**
   * @brief Validate channel pair index
   * @param channelPair Channel pair index
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateChannelPair(int channelPair);

  /**
   * @brief Validate probe configuration
   * @param digitalProbe1 Digital probe 1 setting
   * @param digitalProbe2 Digital probe 2 setting
   * @param analogProbe Analog probe setting
   * @return DecoderResult indicating validation result
   */
  static DecoderResult ValidateProbeConfiguration(uint8_t digitalProbe1,
                                                  uint8_t digitalProbe2,
                                                  uint8_t analogProbe);

 private:
  /**
   * @brief Check if value is within valid range
   * @param value Value to check
   * @param min Minimum valid value
   * @param max Maximum valid value
   * @param name Name of value for error reporting
   * @return true if valid, false otherwise
   */
  static bool IsInRange(uint32_t value, uint32_t min, uint32_t max,
                        const std::string &name);

  /**
   * @brief Check if size is sufficient for required data
   * @param availableSize Available size
   * @param requiredSize Required size
   * @param description Description for error reporting
   * @return true if sufficient, false otherwise
   */
  static bool IsSizeSufficient(size_t availableSize, size_t requiredSize,
                               const std::string &description);
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline DecoderResult DataValidator::ValidateRawData(const uint8_t *data,
                                                    size_t size)
{
  if (data == nullptr) {
    DecoderLogger::LogError("DataValidator", "Raw data pointer is null");
    return DecoderResult::CorruptedData;
  }

  if (size < PSD1Constants::Validation::kMinimumDataSize) {
    DecoderLogger::LogError(
        "DataValidator",
        "Raw data size too small: " + std::to_string(size) +
            " bytes (minimum: " +
            std::to_string(PSD1Constants::Validation::kMinimumDataSize) + ")");
    return DecoderResult::InsufficientData;
  }

  if (size % PSD1Constants::kWordSize != 0) {
    DecoderLogger::LogError("DataValidator",
                            "Raw data size not aligned to word boundary: " +
                                std::to_string(size) + " bytes");
    return DecoderResult::CorruptedData;
  }

  return DecoderResult::Success;
}

inline DecoderResult DataValidator::ValidateChannelPair(int channelPair)
{
  if (channelPair < 0 ||
      channelPair >=
          static_cast<int>(PSD1Constants::Validation::kMaxChannelPairs)) {
    DecoderLogger::LogError("DataValidator", "Invalid channel pair: " +
                                                 std::to_string(channelPair));
    return DecoderResult::InvalidChannelPair;
  }
  return DecoderResult::Success;
}

inline bool DataValidator::IsInRange(uint32_t value, uint32_t min, uint32_t max,
                                     const std::string &name)
{
  if (value < min || value > max) {
    DecoderLogger::LogError("DataValidator",
                            name + " value " + std::to_string(value) +
                                " out of range [" + std::to_string(min) + ", " +
                                std::to_string(max) + "]");
    return false;
  }
  return true;
}

inline bool DataValidator::IsSizeSufficient(size_t availableSize,
                                            size_t requiredSize,
                                            const std::string &description)
{
  if (availableSize < requiredSize) {
    DecoderLogger::LogError("DataValidator",
                            description + " insufficient size: available=" +
                                std::to_string(availableSize) +
                                ", required=" + std::to_string(requiredSize));
    return false;
  }
  return true;
}

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DATAVALIDATOR_HPP