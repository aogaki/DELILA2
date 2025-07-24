#ifndef PSD2CONSTANTS_HPP
#define PSD2CONSTANTS_HPP

#include <cstddef>
#include <cstdint>

namespace DELILA
{
namespace Digitizer
{
namespace PSD2Constants
{

// ============================================================================
// Word Size and Basic Constants
// ============================================================================
constexpr size_t kWordSize = 8;  // 64-bit word size in bytes

// ============================================================================
// Header Constants
// ============================================================================
namespace Header
{
// Header type identification
constexpr uint64_t kTypeMask = 0xF;
constexpr int kTypeShift = 60;
constexpr uint64_t kTypeData = 0x2;

// Fail check
constexpr int kFailCheckShift = 56;
constexpr uint64_t kFailCheckMask = 0x1;

// Aggregate counter
constexpr int kAggregateCounterShift = 32;
constexpr uint64_t kAggregateCounterMask = 0xFFFF;

// Total size
constexpr uint64_t kTotalSizeMask = 0xFFFFFFFF;
}  // namespace Header

// ============================================================================
// Event Data Constants
// ============================================================================
namespace Event
{
// Channel
constexpr int kChannelShift = 56;
constexpr uint64_t kChannelMask = 0x7F;

// Timestamp
constexpr uint64_t kTimeStampMask = 0xFFFFFFFFFFFF;

// Flags
constexpr int kLastWordShift = 63;
constexpr int kWaveformFlagShift = 62;

// Flags - Low Priority
constexpr int kFlagsLowPriorityShift = 50;
constexpr uint64_t kFlagsLowPriorityMask = 0x7FF;

// Flags - High Priority
constexpr int kFlagsHighPriorityShift = 42;
constexpr uint64_t kFlagsHighPriorityMask = 0xFF;

// Energy Short
constexpr int kEnergyShortShift = 26;
constexpr uint64_t kEnergyShortMask = 0xFFFF;

// Fine Time
constexpr int kFineTimeShift = 16;
constexpr uint64_t kFineTimeMask = 0x3FF;
constexpr double kFineTimeScale = 1024.0;

// Energy (Long)
constexpr uint64_t kEnergyMask = 0xFFFF;
}  // namespace Event

// ============================================================================
// Waveform Constants
// ============================================================================
namespace Waveform
{
// Waveform header checks
constexpr int kWaveformCheck1Shift = 63;
constexpr int kWaveformCheck2Shift = 60;
constexpr uint64_t kWaveformCheck2Mask = 0x7;

// Time resolution
constexpr int kTimeResolutionShift = 44;
constexpr uint64_t kTimeResolutionMask = 0x3;

// Trigger threshold
constexpr int kTriggerThresholdShift = 28;
constexpr uint64_t kTriggerThresholdMask = 0xFFFF;

// Waveform words count
constexpr uint64_t kWaveformWordsMask = 0xFFF;

// Probe configuration
constexpr uint64_t kAnalogProbeMask = 0x3FFF;
constexpr uint64_t kDigitalProbeMask = 0x1;

// Probe type positions in header
constexpr int kDigitalProbe4TypeShift = 24;
constexpr int kDigitalProbe3TypeShift = 20;
constexpr int kDigitalProbe2TypeShift = 16;
constexpr int kDigitalProbe1TypeShift = 12;
constexpr int kAnalogProbe2TypeShift = 6;
constexpr int kAnalogProbe1TypeShift = 0;
constexpr uint64_t kProbeTypeMask = 0xF;
constexpr uint64_t kAnalogProbeTypeMask = 0x7;

// Analog probe configuration
constexpr int kAP1SignedShift = 3;
constexpr int kAP1MulFactorShift = 4;
constexpr int kAP2SignedShift = 9;
constexpr int kAP2MulFactorShift = 10;
constexpr uint64_t kAPMulFactorMask = 0x3;

// Waveform point decoding
constexpr int kAnalogProbe2Shift = 16;
constexpr int kDigitalProbe1Shift = 14;
constexpr int kDigitalProbe2Shift = 15;
constexpr int kDigitalProbe3Shift = 30;
constexpr int kDigitalProbe4Shift = 31;
}  // namespace Waveform

// ============================================================================
// Start/Stop Signal Constants
// ============================================================================
namespace StartStop
{
// Start signal pattern
constexpr uint64_t kStartFirstWordType = 0x3;
constexpr uint64_t kStartFirstWordSubType = 0x0;
constexpr uint64_t kStartSecondWordType = 0x2;
constexpr uint64_t kStartThirdWordType = 0x1;
constexpr uint64_t kStartFourthWordType = 0x1;

// Stop signal pattern
constexpr uint64_t kStopFirstWordType = 0x3;
constexpr uint64_t kStopFirstWordSubType = 0x2;
constexpr uint64_t kStopSecondWordType = 0x0;
constexpr uint64_t kStopThirdWordType = 0x1;

// Common bit positions
constexpr int kSignalTypeShift = 60;
constexpr int kSignalSubTypeShift = 56;
constexpr uint64_t kSignalTypeMask = 0xF;

// Dead time in stop signal
constexpr uint64_t kDeadTimeMask = 0xFFFFFFFF;
constexpr int kDeadTimeScale = 8;  // ns
}  // namespace StartStop

// ============================================================================
// Validation Constants
// ============================================================================
namespace Validation
{
constexpr size_t kMinimumDataSize = 3 * kWordSize;   // Minimum for stop signal
constexpr size_t kStartSignalSize = 4 * kWordSize;   // Start signal size
constexpr size_t kStopSignalSize = 3 * kWordSize;    // Stop signal size
constexpr size_t kMinimumEventSize = 2 * kWordSize;  // Minimum event size
constexpr size_t kMaxChannelNumber = 127;            // Maximum channel number
constexpr size_t kMaxWaveformSamples = 65536;  // Reasonable waveform size limit
}  // namespace Validation

// ============================================================================
// Multiplication Factor Encoding
// ============================================================================
namespace MultiplicationFactor
{
constexpr uint32_t kFactor1 = 0;   // Encoded as 0
constexpr uint32_t kFactor4 = 1;   // Encoded as 1
constexpr uint32_t kFactor8 = 2;   // Encoded as 2
constexpr uint32_t kFactor16 = 3;  // Encoded as 3
}  // namespace MultiplicationFactor

}  // namespace PSD2Constants
}  // namespace Digitizer
}  // namespace DELILA

#endif  // PSD2CONSTANTS_HPP