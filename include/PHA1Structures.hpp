#ifndef PHA1STRUCTURES_HPP
#define PHA1STRUCTURES_HPP

#include <cstdint>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// PHA1 Format Structures
// ============================================================================

// Board level (4 words) - PHA1 specific
struct PHA1BoardHeaderInfo {
  uint32_t aggregateSize;     // [0:27] size in 32-bit words
  uint8_t dualChannelMask;    // [0:7] which channel pairs active
  uint16_t lvdsPattern;       // [8:22] LVDS pattern
  bool boardFailFlag;         // [26] board failure
  uint8_t boardId;            // [27:31] board identifier
  uint32_t aggregateCounter;  // [0:22] aggregate counter
  uint32_t boardTimeTag;      // [0:31] board time tag
};

// Dual channel level (2 words) - PHA1 specific
struct PHA1DualChannelInfo {
  uint32_t aggregateSize;   // [0:30] dual channel size (different from PSD1!)
  uint16_t numSamplesWave;  // [0:15] waveform samples/8
  uint8_t digitalProbe;     // [16:19] DP: single digital probe selection
  uint8_t analogProbe2;     // [20:21] AP2: analog probe 2 selection
  uint8_t analogProbe1;     // [22:23] AP1: analog probe 1 selection
  uint8_t extraOption;      // [24:26] extras format (usually 0b010)
  bool samplesEnabled;      // [27] ES: waveform enabled
  bool extras2Enabled;      // [28] E2: extras 2 enabled
  bool timeEnabled;         // [29] ET: time tag enabled
  bool energyEnabled;       // [30] EE: energy enabled
  bool dualTraceEnabled;    // [31] DT: dual trace enabled
};

// PHA1 Event Data structure
struct PHA1EventInfo {
  uint32_t triggerTimeTag;  // [0:30] trigger time tag
  bool isOddChannel;        // [31] channel pair flag
  uint32_t extrasWord;      // [0:31] extras word (format depends on extraOption)
  uint16_t energy;          // [0:14] energy value
  bool pileupFlag;          // [15] pileup flag
  uint16_t extra;           // [16:25] extra data
  
  // Decoded extras (for extraOption = 0b010)
  uint16_t fineTimeStamp;   // [0:9] fine time stamp
  uint16_t extendedTime;    // [16:31] extended time stamp
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // PHA1STRUCTURES_HPP