#include "EventGenerator.hpp"
#include <chrono>

namespace delila {
namespace test {

EventGenerator::EventGenerator(uint32_t sourceId) 
    : sourceId(sourceId),
      rng(std::chrono::steady_clock::now().time_since_epoch().count()),
      minEnergy(100), maxEnergy(65000),
      minModule(0), maxModule(15),
      minChannel(0), maxChannel(15),
      timeResolution(1000),
      currentTimestamp(0.0) {
    
    UpdateDistributions();
}

EventGenerator::~EventGenerator() = default;

EventDataBatch EventGenerator::GenerateBatch(uint32_t eventCount, uint64_t sequenceNumber) {
    EventDataBatch batch(sourceId, sequenceNumber);
    batch.Reserve(eventCount);
    
    for (uint32_t i = 0; i < eventCount; ++i) {
        batch.AddEvent(GenerateEvent());
    }
    
    return batch;
}

DELILA::Digitizer::EventData EventGenerator::GenerateEvent() {
    DELILA::Digitizer::EventData event;
    
    // Generate timestamp (increasing)
    event.timestamp_ns = GetNextTimestamp();
    
    // Generate energy values
    event.energy = energyDist(rng);
    event.energy_short = energyShortDist(rng);
    
    // Generate module and channel
    event.module = moduleDist(rng);
    event.channel = channelDist(rng);
    
    // Set time resolution
    event.time_resolution = timeResolution;
    
    // Generate flags
    event.flags = flagsDist(rng);
    
    return event;
}

void EventGenerator::SetEnergyRange(uint32_t minEnergy, uint32_t maxEnergy) {
    this->minEnergy = minEnergy;
    this->maxEnergy = maxEnergy;
    UpdateDistributions();
}

void EventGenerator::SetModuleChannelRange(uint32_t minModule, uint32_t maxModule, 
                                         uint32_t minChannel, uint32_t maxChannel) {
    this->minModule = minModule;
    this->maxModule = maxModule;
    this->minChannel = minChannel;
    this->maxChannel = maxChannel;
    UpdateDistributions();
}

void EventGenerator::SetTimeResolution(uint32_t resolution) {
    this->timeResolution = resolution;
}

void EventGenerator::UpdateDistributions() {
    energyDist = std::uniform_int_distribution<uint32_t>(minEnergy, maxEnergy);
    energyShortDist = std::uniform_int_distribution<uint32_t>(minEnergy / 2, maxEnergy / 2);
    moduleDist = std::uniform_int_distribution<uint32_t>(minModule, maxModule);
    channelDist = std::uniform_int_distribution<uint32_t>(minChannel, maxChannel);
    flagsDist = std::uniform_int_distribution<uint32_t>(0, 0xFFFF); // 16-bit flags
}

double EventGenerator::GetNextTimestamp() {
    // Increment timestamp by a small random amount to simulate realistic timing
    std::uniform_real_distribution<double> timeDist(1.0, 10.0); // 1-10 ns increment
    currentTimestamp += timeDist(rng);
    return currentTimestamp;
}

}} // namespace delila::test