#ifndef EVENTDATA_HPP
#define EVENTDATA_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Event data structure for digitizer events
 *
 * This class represents a single event from a digitizer, containing
 * timing information, energy measurements, and optional waveform data.
 */
class EventData
{
 public:
  // Constructors and Destructor
  explicit EventData(size_t waveformSize = 0);
  ~EventData() = default;

  // Copy semantics
  EventData(const EventData &other);
  EventData &operator=(const EventData &other);

  // Move semantics
  EventData(EventData &&other) noexcept;
  EventData &operator=(EventData &&other) noexcept;

  // Waveform management
  void ResizeWaveform(size_t size);
  void ClearWaveform();

  // Display methods
  void Print() const;
  void PrintSummary() const;
  void PrintWaveform(size_t maxSamples = 10) const;

  // Public data members
  double timeStampNs;
  size_t waveformSize;
  std::vector<int32_t> analogProbe1;
  std::vector<int32_t> analogProbe2;
  std::vector<uint8_t> digitalProbe1;
  std::vector<uint8_t> digitalProbe2;
  std::vector<uint8_t> digitalProbe3;
  std::vector<uint8_t> digitalProbe4;
  uint16_t energy;
  uint16_t energyShort;
  uint8_t module;
  uint8_t channel;
  uint8_t timeResolution;
  uint8_t analogProbe1Type;
  uint8_t analogProbe2Type;
  uint8_t digitalProbe1Type;
  uint8_t digitalProbe2Type;
  uint8_t digitalProbe3Type;
  uint8_t digitalProbe4Type;
  uint8_t downSampleFactor;

  // NEW: Flags field for status information (dig1/dig2)
  uint64_t flags;

  // Flag bit definitions for PSD1/PSD2
  static constexpr uint64_t FLAG_PILEUP = 0x01;          // Pileup detected
  static constexpr uint64_t FLAG_TRIGGER_LOST = 0x02;    // Trigger lost
  static constexpr uint64_t FLAG_OVER_RANGE = 0x04;      // Signal saturation
  static constexpr uint64_t FLAG_1024_TRIGGER = 0x08;    // 1024 trigger count
  static constexpr uint64_t FLAG_N_LOST_TRIGGER = 0x10;  // N lost triggers

  // Optional: Helper methods for flag checking
  bool HasPileup() const { return (flags & FLAG_PILEUP) != 0; }
  bool HasTriggerLost() const { return (flags & FLAG_TRIGGER_LOST) != 0; }
  bool HasOverRange() const { return (flags & FLAG_OVER_RANGE) != 0; }

 private:
  // Helper method for copying data
  void CopyFrom(const EventData &other);
};

// Modern type alias
using EventData_t = EventData;

// Legacy typedef for backward compatibility
using EVENTDATA_t = EventData;

}  // namespace Digitizer
}  // namespace DELILA

#endif  // EVENTDATA_HPP