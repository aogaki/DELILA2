#ifndef PHA1CONSTANTS_HPP
#define PHA1CONSTANTS_HPP

#include <cstdint>
#include <cstddef>

namespace DELILA
{
namespace Digitizer
{
namespace PHA1Constants
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
// Dual channel aggregate size (PHA1 uses bit[0:30])
constexpr uint32_t kDualChannelSizeMask = 0x7FFFFFFF;  // [0:30]
constexpr int kDualChannelHeaderShift = 31;

// Waveform samples
constexpr uint32_t kNumSamplesWaveMask = 0xFFFF;  // [0:15]

// Digital probe (PHA1 has single DP at bit[16:19])
constexpr int kDigitalProbeShift = 16;
constexpr uint32_t kDigitalProbeMask = 0xF;  // [16:19]

// Analog probes (PHA1 has AP2 and AP1)
constexpr int kAnalogProbe2Shift = 20;
constexpr uint32_t kAnalogProbe2Mask = 0x3;  // [20:21]
constexpr int kAnalogProbe1Shift = 22;
constexpr uint32_t kAnalogProbe1Mask = 0x3;  // [22:23]

// Extra option
constexpr int kExtraOptionShift = 24;
constexpr uint32_t kExtraOptionMask = 0x7;  // [24:26]

// Enable flags (PHA1 specific)
constexpr int kSamplesEnabledShift = 27;  // ES
constexpr int kExtras2EnabledShift = 28;  // E2 (Extras 2 enabled)
constexpr int kTimeEnabledShift = 29;     // ET
constexpr int kEnergyEnabledShift = 30;   // EE (Energy enabled)
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

// Energy word bit masks (PHA-specific)
constexpr uint32_t kEnergyMask = 0x7FFF;  // [0:14] Energy value
constexpr int kPileupFlagShift = 15;      // [15] Pileup flag
constexpr int kExtraShift = 16;           // [16:25] Extra
constexpr uint32_t kExtraMask = 0x3FF;    // [16:25] Extra mask
}  // namespace Event

// ============================================================================
// Waveform Constants
// ============================================================================
namespace Waveform
{
// Waveform data bit masks (16-bit samples, 2 per 32-bit word)
constexpr uint32_t kAnalogSampleMask = 0x3FFF;  // [0:13] or [16:29]
constexpr int kDigitalProbeWaveShift = 14;      // [14] or [30] (single DP)
constexpr int kTriggerFlagShift = 15;           // [15] or [31] (Tn: trigger flag)
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

}  // namespace PHA1Constants
}  // namespace Digitizer
}  // namespace DELILA

#endif  // PHA1CONSTANTS_HPP