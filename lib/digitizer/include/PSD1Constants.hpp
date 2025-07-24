#ifndef PSD1CONSTANTS_HPP
#define PSD1CONSTANTS_HPP

#include <cstdint>

namespace DELILA
{
namespace Digitizer
{
namespace PSD1Constants
{

// ============================================================================
// Word Size and Basic Constants
// ============================================================================
constexpr size_t kWordSize = 4;  // 32-bit word size in bytes

// ============================================================================
// Board Header Constants
// ============================================================================
namespace BoardHeader
{
// Header type identification
constexpr uint32_t kTypeMask = 0xF;
constexpr int kTypeShift = 28;
constexpr uint32_t kTypeData = 0xA;  // 0b1010

// Board aggregate size
constexpr uint32_t kAggregateSizeMask = 0x0FFFFFFF;  // [0:27]

// Dual channel mask
constexpr int kDualChannelMaskShift = 0;
constexpr uint32_t kDualChannelMaskMask = 0xFF;  // [0:7]

// LVDS pattern
constexpr int kLVDSPatternShift = 8;
constexpr uint32_t kLVDSPatternMask = 0x7FFF;  // [8:22]

// Board failure flag
constexpr int kBoardFailShift = 26;
constexpr uint32_t kBoardFailMask = 0x1;

// Board ID
constexpr int kBoardIdShift = 27;
constexpr uint32_t kBoardIdMask = 0x1F;  // [27:31]

// Board counter
constexpr uint32_t kBoardCounterMask = 0x7FFFFF;  // [0:22]

// Header size in words
constexpr size_t kHeaderSizeWords = 4;
}  // namespace BoardHeader

// ============================================================================
// Channel Header Constants
// ============================================================================
namespace ChannelHeader
{
// Dual channel aggregate size
constexpr uint32_t kDualChannelSizeMask = 0x3FFFFF;  // [0:21]
constexpr int kDualChannelHeaderShift = 31;

// Waveform samples
constexpr uint32_t kNumSamplesWaveMask = 0xFFFF;  // [0:15]

// Digital probes
constexpr int kDigitalProbe1Shift = 16;
constexpr uint32_t kDigitalProbe1Mask = 0x7;  // [16:18]
constexpr int kDigitalProbe2Shift = 19;
constexpr uint32_t kDigitalProbe2Mask = 0x7;  // [19:21]

// Analog probe
constexpr int kAnalogProbeShift = 22;
constexpr uint32_t kAnalogProbeMask = 0x3;  // [22:23]

// Extra option
constexpr int kExtraOptionShift = 24;
constexpr uint32_t kExtraOptionMask = 0x7;  // [24:26]

// Enable flags
constexpr int kSamplesEnabledShift = 27;  // ES
constexpr int kExtrasEnabledShift = 28;   // EE
constexpr int kTimeEnabledShift = 29;     // ET
constexpr int kChargeEnabledShift = 30;   // EQ
constexpr int kDualTraceShift = 31;       // DT

// Header size in words
constexpr size_t kHeaderSizeWords = 2;
}  // namespace ChannelHeader

// ============================================================================
// Event Data Constants
// ============================================================================
namespace Event
{
// Trigger time tag
constexpr uint32_t kTriggerTimeTagMask = 0x7FFFFFFF;  // [0:30]
constexpr int kChannelFlagShift = 31;                 // [31] 0=even, 1=odd

// Extras word bit masks (for extra option 0b010)
constexpr uint32_t kFineTimeStampMask = 0x3FF;  // [0:9]
constexpr int kFlagsShift = 10;
constexpr uint32_t kFlagsMask = 0x3F;  // [10:15]
constexpr int kExtendedTimeShift = 16;
constexpr uint32_t kExtendedTimeMask = 0xFFFF;  // [16:31]

// Charge word bit masks
constexpr uint32_t kChargeShortMask = 0x7FFF;  // [0:14]
constexpr int kPileupFlagShift = 15;
constexpr int kChargeLongShift = 16;
constexpr uint32_t kChargeLongMask = 0xFFFF;  // [16:31]
}  // namespace Event

// ============================================================================
// Waveform Constants
// ============================================================================
namespace Waveform
{
// Waveform data bit masks (16-bit samples, 2 per 32-bit word)
constexpr uint32_t kAnalogSampleMask = 0x3FFF;  // [0:13] or [16:29]
constexpr int kDigitalProbe1WaveShift = 14;     // [14] or [30]
constexpr int kDigitalProbe2WaveShift = 15;     // [15] or [31]
constexpr int kSecondSampleShift = 16;  // Second sample starts at bit 16

// Sample packing
constexpr int kSamplesPerWord = 2;
constexpr int kSamplesPerGroup = 8;       // numSamplesWave represents samples/8
constexpr uint32_t kSampleMask = 0xFFFF;  // 16-bit samples
}  // namespace Waveform

// ============================================================================
// Data Type Enums
// ============================================================================
enum class ProbeType : uint8_t {
  Trigger = 0,
  Trapezoid = 1,
  Energy = 2,
  Timestamp = 3,
  Reserved4 = 4,
  Reserved5 = 5,
  Reserved6 = 6,
  Reserved7 = 7
};

enum class ExtraOption : uint8_t {
  ExtendedTimestamp = 0,   // 0b000 - Extended timestamp only
  ExtendedTimestamp1 = 1,  // 0b001 - Extended timestamp only
  ExtendedFlagsFineTT =
      2,        // 0b010 - Extended timestamp + flags + fine timestamp
  Option3 = 3,  // 0b011 - Other format
  Option4 = 4,  // 0b100 - Other format
  Option5 = 5,  // 0b101 - Other format
  Option6 = 6,  // 0b110 - Other format
  Option7 = 7   // 0b111 - Other format
};

// Extra option format constants
namespace ExtraFormats
{
constexpr uint8_t kExtendedTimestampOnly = 0;  // Options 0b000 and 0b001
constexpr uint8_t kExtendedTimestampOnly1 = 1;
constexpr uint8_t kExtendedFlagsFineTT = 2;  // Option 0b010
}  // namespace ExtraFormats

// ============================================================================
// Validation Constants
// ============================================================================
namespace Validation
{
constexpr size_t kMinimumDataSize = 4 * kWordSize;  // Minimum for board header
constexpr size_t kMinimumEventSize =
    16 * kWordSize;                     // Minimum for meaningful event
constexpr size_t kMaxChannelPairs = 8;  // Maximum channel pairs per board
constexpr size_t kMaxBoardId = 31;      // Maximum board ID value
constexpr size_t kMaxWaveformSamples = 65536;  // Reasonable waveform size limit
}  // namespace Validation

}  // namespace PSD1Constants
}  // namespace Digitizer
}  // namespace DELILA

#endif  // PSD1CONSTANTS_HPP