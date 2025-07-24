#ifndef PSD1STRUCTURES_HPP
#define PSD1STRUCTURES_HPP

#include <cstdint>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// PSD1 Format Structures
// ============================================================================

// Board level (4 words) - PSD1 specific
struct PSD1BoardHeaderInfo {
  uint32_t aggregateSize;     // [0:27] size in 32-bit words
  uint8_t dualChannelMask;    // [0:7] which channel pairs active
  uint16_t lvdsPattern;       // [8:22] LVDS pattern
  bool boardFailFlag;         // [26] board failure
  uint8_t boardId;            // [27:31] board identifier
  uint32_t aggregateCounter;  // [0:22] aggregate counter
  uint32_t boardTimeTag;      // [0:31] board time tag
};

// Dual channel level (2 words) - PSD1 specific
struct PSD1DualChannelInfo {
  uint32_t aggregateSize;   // [0:21] dual channel size
  uint16_t numSamplesWave;  // [0:15] waveform samples/8
  uint8_t digitalProbe1;    // [16:18] DP1 selection
  uint8_t digitalProbe2;    // [19:21] DP2 selection
  uint8_t analogProbe;      // [22:23] AP selection
  uint8_t extraOption;      // [24:26] extras format (usually 0b010)
  bool samplesEnabled;      // [27] ES: waveform enabled
  bool extrasEnabled;       // [28] EE: extras enabled
  bool timeEnabled;         // [29] ET: time tag enabled
  bool chargeEnabled;       // [30] EQ: charge enabled
  bool dualTraceEnabled;    // [31] DT: dual trace enabled
};

// Legacy aliases for backward compatibility
using BoardHeaderInfo = PSD1BoardHeaderInfo;
using DualChannelInfo = PSD1DualChannelInfo;

}  // namespace Digitizer
}  // namespace DELILA

#endif  // PSD1STRUCTURES_HPP