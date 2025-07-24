#ifndef PSD2STRUCTURES_HPP
#define PSD2STRUCTURES_HPP

#include <cstdint>
#include <vector>

#include "PSD2Constants.hpp"

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Header Information Structure
// ============================================================================
struct PSD2HeaderInfo {
  uint64_t headerType;        // Header type (should be 0x2 for data)
  bool failCheck;             // Board failure flag
  uint64_t aggregateCounter;  // Aggregate counter
  uint32_t totalSize;         // Total size in words

  PSD2HeaderInfo()
      : headerType(0), failCheck(false), aggregateCounter(0), totalSize(0)
  {
  }
};

// ============================================================================
// Waveform Configuration Structure
// ============================================================================
struct WaveformConfig {
  bool ap1IsSigned;       // Analog probe 1 is signed
  bool ap2IsSigned;       // Analog probe 2 is signed
  uint32_t ap1MulFactor;  // Analog probe 1 multiplication factor
  uint32_t ap2MulFactor;  // Analog probe 2 multiplication factor

  WaveformConfig()
      : ap1IsSigned(false), ap2IsSigned(false), ap1MulFactor(1), ap2MulFactor(1)
  {
  }
};

// ============================================================================
// Waveform Header Information
// ============================================================================
struct WaveformHeaderInfo {
  bool headerValid;           // Waveform header validation flags
  uint8_t timeResolution;     // Time resolution setting
  uint16_t triggerThreshold;  // Trigger threshold
  uint64_t nWordsWaveform;    // Number of waveform words

  // Probe types
  uint8_t digitalProbe4Type;
  uint8_t digitalProbe3Type;
  uint8_t digitalProbe2Type;
  uint8_t digitalProbe1Type;
  uint8_t analogProbe2Type;
  uint8_t analogProbe1Type;

  WaveformHeaderInfo()
      : headerValid(false),
        timeResolution(0),
        triggerThreshold(0),
        nWordsWaveform(0),
        digitalProbe4Type(0),
        digitalProbe3Type(0),
        digitalProbe2Type(0),
        digitalProbe1Type(0),
        analogProbe2Type(0),
        analogProbe1Type(0)
  {
  }
};

// ============================================================================
// Event Information Structure
// ============================================================================
struct PSD2EventInfo {
  uint8_t channel;        // Channel number
  uint64_t rawTimeStamp;  // Raw timestamp from first word
  uint64_t flags;         // Combined flags from second word
  uint16_t energyShort;   // Short gate energy
  uint16_t energy;        // Long gate energy
  uint64_t fineTime;      // Fine time correction
  double timeStampNs;     // Final timestamp in nanoseconds
  bool hasWaveform;       // Whether waveform data is present

  PSD2EventInfo()
      : channel(0),
        rawTimeStamp(0),
        flags(0),
        energyShort(0),
        energy(0),
        fineTime(0),
        timeStampNs(0.0),
        hasWaveform(false)
  {
  }
};

// ============================================================================
// Start/Stop Signal Information
// ============================================================================
struct StartStopInfo {
  enum class Type { Unknown = 0, Start = 1, Stop = 2 };

  Type signalType;
  uint32_t deadTime;  // Dead time in stop signal (in ns)

  StartStopInfo() : signalType(Type::Unknown), deadTime(0) {}
};

// ============================================================================
// Decoder State Structure
// ============================================================================
struct PSD2DecoderState {
  uint64_t lastCounter;  // Last aggregate counter value
  uint64_t eventCount;   // Total number of events processed
  uint64_t errorCount;   // Total number of errors encountered
  bool isRunning;        // Whether decoder is in running state
  bool isInitialized;    // Whether decoder has been initialized

  PSD2DecoderState()
      : lastCounter(0),
        eventCount(0),
        errorCount(0),
        isRunning(false),
        isInitialized(false)
  {
  }
};

// ============================================================================
// Processing Result Enumeration
// ============================================================================
enum class ProcessingResult {
  Success = 0,
  InvalidHeader,
  InvalidSize,
  InvalidData,
  InsufficientData,
  UnknownSignal,
  WaveformError,
  ValidationError
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // PSD2STRUCTURES_HPP