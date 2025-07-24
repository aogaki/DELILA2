#include "EventData.hpp"

#include <algorithm>
#include <iostream>

namespace DELILA
{
namespace Digitizer
{

// Constructor
EventData::EventData(size_t waveformSize)
    : timeStampNs(0.0),
      waveformSize(0),
      energy(0),
      energyShort(0),
      module(0),
      channel(0),
      timeResolution(0),
      analogProbe1Type(0),
      analogProbe2Type(0),
      digitalProbe1Type(0),
      digitalProbe2Type(0),
      digitalProbe3Type(0),
      digitalProbe4Type(0),
      downSampleFactor(0),
      flags(0)
{
  if (waveformSize > 0) {
    ResizeWaveform(waveformSize);
  }
}

// Copy constructor
EventData::EventData(const EventData &other) { CopyFrom(other); }

// Copy assignment operator
EventData &EventData::operator=(const EventData &other)
{
  if (this != &other) {
    CopyFrom(other);
  }
  return *this;
}

// Move constructor
EventData::EventData(EventData &&other) noexcept
    : timeStampNs(other.timeStampNs),
      waveformSize(other.waveformSize),
      analogProbe1(std::move(other.analogProbe1)),
      analogProbe2(std::move(other.analogProbe2)),
      digitalProbe1(std::move(other.digitalProbe1)),
      digitalProbe2(std::move(other.digitalProbe2)),
      digitalProbe3(std::move(other.digitalProbe3)),
      digitalProbe4(std::move(other.digitalProbe4)),
      energy(other.energy),
      energyShort(other.energyShort),
      module(other.module),
      channel(other.channel),
      timeResolution(other.timeResolution),
      analogProbe1Type(other.analogProbe1Type),
      analogProbe2Type(other.analogProbe2Type),
      digitalProbe1Type(other.digitalProbe1Type),
      digitalProbe2Type(other.digitalProbe2Type),
      digitalProbe3Type(other.digitalProbe3Type),
      digitalProbe4Type(other.digitalProbe4Type),
      downSampleFactor(other.downSampleFactor),
      flags(other.flags)
{
  // Reset other object
  other.timeStampNs = 0.0;
  other.waveformSize = 0;
  other.energy = 0;
  other.energyShort = 0;
  other.module = 0;
  other.channel = 0;
  other.timeResolution = 0;
  other.analogProbe1Type = 0;
  other.analogProbe2Type = 0;
  other.digitalProbe1Type = 0;
  other.digitalProbe2Type = 0;
  other.digitalProbe3Type = 0;
  other.digitalProbe4Type = 0;
  other.downSampleFactor = 0;
  other.flags = 0;
}

// Move assignment operator
EventData &EventData::operator=(EventData &&other) noexcept
{
  if (this != &other) {
    // Move public members
    timeStampNs = other.timeStampNs;
    waveformSize = other.waveformSize;
    analogProbe1 = std::move(other.analogProbe1);
    analogProbe2 = std::move(other.analogProbe2);
    digitalProbe1 = std::move(other.digitalProbe1);
    digitalProbe2 = std::move(other.digitalProbe2);
    digitalProbe3 = std::move(other.digitalProbe3);
    digitalProbe4 = std::move(other.digitalProbe4);
    energy = other.energy;
    energyShort = other.energyShort;
    module = other.module;
    channel = other.channel;
    timeResolution = other.timeResolution;
    analogProbe1Type = other.analogProbe1Type;
    analogProbe2Type = other.analogProbe2Type;
    digitalProbe1Type = other.digitalProbe1Type;
    digitalProbe2Type = other.digitalProbe2Type;
    digitalProbe3Type = other.digitalProbe3Type;
    digitalProbe4Type = other.digitalProbe4Type;
    downSampleFactor = other.downSampleFactor;
    flags = other.flags;

    // Reset other object
    other.timeStampNs = 0.0;
    other.waveformSize = 0;
    other.energy = 0;
    other.energyShort = 0;
    other.module = 0;
    other.channel = 0;
    other.timeResolution = 0;
    other.analogProbe1Type = 0;
    other.analogProbe2Type = 0;
    other.digitalProbe1Type = 0;
    other.digitalProbe2Type = 0;
    other.digitalProbe3Type = 0;
    other.digitalProbe4Type = 0;
    other.downSampleFactor = 0;
    other.flags = 0;
  }
  return *this;
}

void EventData::ResizeWaveform(size_t size)
{
  waveformSize = size;

  // Resize vectors
  analogProbe1.resize(size);
  analogProbe2.resize(size);
  digitalProbe1.resize(size);
  digitalProbe2.resize(size);
  digitalProbe3.resize(size);
  digitalProbe4.resize(size);
}

void EventData::ClearWaveform() { ResizeWaveform(0); }

// ============================================================================
// Display Methods
// ============================================================================

void EventData::Print() const
{
  std::cout << "\n=== Event Data ===" << std::endl;
  std::cout << "Timestamp (ns): " << timeStampNs << std::endl;
  std::cout << "Module: " << static_cast<int>(module) << std::endl;
  std::cout << "Channel: " << static_cast<int>(channel) << std::endl;
  std::cout << "Energy: " << energy << std::endl;
  std::cout << "Energy Short: " << energyShort << std::endl;
  std::cout << "Time Resolution: " << static_cast<int>(timeResolution)
            << std::endl;
  std::cout << "Down Sample Factor: " << static_cast<int>(downSampleFactor)
            << std::endl;
  std::cout << "Flags: 0x" << std::hex << flags << std::dec;
  if (flags != 0) {
    std::cout << " (";
    if (HasPileup()) std::cout << "PILEUP ";
    if (HasTriggerLost()) std::cout << "TRIGGER_LOST ";
    if (HasOverRange()) std::cout << "OVER_RANGE ";
    std::cout << ")";
  }
  std::cout << std::endl;

  std::cout << "\nProbe Types:" << std::endl;
  std::cout << "  Analog Probe 1: " << static_cast<int>(analogProbe1Type)
            << std::endl;
  std::cout << "  Analog Probe 2: " << static_cast<int>(analogProbe2Type)
            << std::endl;
  std::cout << "  Digital Probe 1: " << static_cast<int>(digitalProbe1Type)
            << std::endl;
  std::cout << "  Digital Probe 2: " << static_cast<int>(digitalProbe2Type)
            << std::endl;
  std::cout << "  Digital Probe 3: " << static_cast<int>(digitalProbe3Type)
            << std::endl;
  std::cout << "  Digital Probe 4: " << static_cast<int>(digitalProbe4Type)
            << std::endl;

  std::cout << "\nWaveform Info:" << std::endl;
  std::cout << "  Size: " << waveformSize << " samples" << std::endl;

  if (waveformSize > 0) {
    std::cout << "  Analog Probe 1 Size: " << analogProbe1.size() << std::endl;
    std::cout << "  Analog Probe 2 Size: " << analogProbe2.size() << std::endl;
    std::cout << "  Digital Probe 1 Size: " << digitalProbe1.size()
              << std::endl;
    std::cout << "  Digital Probe 2 Size: " << digitalProbe2.size()
              << std::endl;
    std::cout << "  Digital Probe 3 Size: " << digitalProbe3.size()
              << std::endl;
    std::cout << "  Digital Probe 4 Size: " << digitalProbe4.size()
              << std::endl;

    PrintWaveform(5);  // Show first 5 samples by default
  }

  std::cout << "==================" << std::endl;
}

void EventData::PrintSummary() const
{
  std::cout << "Event[M" << static_cast<int>(module) << ":Ch"
            << static_cast<int>(channel) << "] Time: " << timeStampNs
            << "ns, Energy: " << energy << ", Samples: " << waveformSize
            << std::endl;
}

void EventData::PrintWaveform(size_t maxSamples) const
{
  if (waveformSize == 0) {
    std::cout << "  No waveform data available" << std::endl;
    return;
  }

  size_t samplesToShow = std::min(maxSamples, waveformSize);

  std::cout << "\nWaveform Data (first " << samplesToShow
            << " samples):" << std::endl;

  // Print analog probe data
  if (!analogProbe1.empty()) {
    std::cout << "  Analog Probe 1: ";
    for (size_t i = 0; i < samplesToShow && i < analogProbe1.size(); ++i) {
      std::cout << analogProbe1[i];
      if (i < samplesToShow - 1 && i < analogProbe1.size() - 1)
        std::cout << ", ";
    }
    if (analogProbe1.size() > samplesToShow) {
      std::cout << " ... (" << (analogProbe1.size() - samplesToShow)
                << " more)";
    }
    std::cout << std::endl;
  }

  if (!analogProbe2.empty()) {
    std::cout << "  Analog Probe 2: ";
    for (size_t i = 0; i < samplesToShow && i < analogProbe2.size(); ++i) {
      std::cout << analogProbe2[i];
      if (i < samplesToShow - 1 && i < analogProbe2.size() - 1)
        std::cout << ", ";
    }
    if (analogProbe2.size() > samplesToShow) {
      std::cout << " ... (" << (analogProbe2.size() - samplesToShow)
                << " more)";
    }
    std::cout << std::endl;
  }

  // Print digital probe data
  if (!digitalProbe1.empty()) {
    std::cout << "  Digital Probe 1: ";
    for (size_t i = 0; i < samplesToShow && i < digitalProbe1.size(); ++i) {
      std::cout << static_cast<int>(digitalProbe1[i]);
      if (i < samplesToShow - 1 && i < digitalProbe1.size() - 1)
        std::cout << ", ";
    }
    if (digitalProbe1.size() > samplesToShow) {
      std::cout << " ... (" << (digitalProbe1.size() - samplesToShow)
                << " more)";
    }
    std::cout << std::endl;
  }

  if (!digitalProbe2.empty()) {
    std::cout << "  Digital Probe 2: ";
    for (size_t i = 0; i < samplesToShow && i < digitalProbe2.size(); ++i) {
      std::cout << static_cast<int>(digitalProbe2[i]);
      if (i < samplesToShow - 1 && i < digitalProbe2.size() - 1)
        std::cout << ", ";
    }
    if (digitalProbe2.size() > samplesToShow) {
      std::cout << " ... (" << (digitalProbe2.size() - samplesToShow)
                << " more)";
    }
    std::cout << std::endl;
  }

  if (!digitalProbe3.empty()) {
    std::cout << "  Digital Probe 3: ";
    for (size_t i = 0; i < samplesToShow && i < digitalProbe3.size(); ++i) {
      std::cout << static_cast<int>(digitalProbe3[i]);
      if (i < samplesToShow - 1 && i < digitalProbe3.size() - 1)
        std::cout << ", ";
    }
    if (digitalProbe3.size() > samplesToShow) {
      std::cout << " ... (" << (digitalProbe3.size() - samplesToShow)
                << " more)";
    }
    std::cout << std::endl;
  }

  if (!digitalProbe4.empty()) {
    std::cout << "  Digital Probe 4: ";
    for (size_t i = 0; i < samplesToShow && i < digitalProbe4.size(); ++i) {
      std::cout << static_cast<int>(digitalProbe4[i]);
      if (i < samplesToShow - 1 && i < digitalProbe4.size() - 1)
        std::cout << ", ";
    }
    if (digitalProbe4.size() > samplesToShow) {
      std::cout << " ... (" << (digitalProbe4.size() - samplesToShow)
                << " more)";
    }
    std::cout << std::endl;
  }
}

void EventData::CopyFrom(const EventData &other)
{
  // Copy all public members
  timeStampNs = other.timeStampNs;
  waveformSize = other.waveformSize;
  analogProbe1 = other.analogProbe1;
  analogProbe2 = other.analogProbe2;
  digitalProbe1 = other.digitalProbe1;
  digitalProbe2 = other.digitalProbe2;
  digitalProbe3 = other.digitalProbe3;
  digitalProbe4 = other.digitalProbe4;
  energy = other.energy;
  energyShort = other.energyShort;
  module = other.module;
  channel = other.channel;
  timeResolution = other.timeResolution;
  analogProbe1Type = other.analogProbe1Type;
  analogProbe2Type = other.analogProbe2Type;
  digitalProbe1Type = other.digitalProbe1Type;
  digitalProbe2Type = other.digitalProbe2Type;
  digitalProbe3Type = other.digitalProbe3Type;
  digitalProbe4Type = other.digitalProbe4Type;
  downSampleFactor = other.downSampleFactor;
  flags = other.flags;
}

}  // namespace Digitizer
}  // namespace DELILA