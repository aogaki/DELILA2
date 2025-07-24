#pragma once

#include "EventDataBatch.hpp"
#include "Common.hpp"
#include <random>
#include <memory>

namespace delila {
namespace test {

class EventGenerator {
public:
    // Constructor
    EventGenerator(uint32_t sourceId);
    
    // Destructor
    ~EventGenerator();
    
    // Generate a batch of events
    EventDataBatch GenerateBatch(uint32_t eventCount, uint64_t sequenceNumber);
    
    // Generate single event
    DELILA::Digitizer::EventData GenerateEvent();
    
    // Set generation parameters
    void SetEnergyRange(uint32_t minEnergy, uint32_t maxEnergy);
    void SetModuleChannelRange(uint32_t minModule, uint32_t maxModule, 
                              uint32_t minChannel, uint32_t maxChannel);
    void SetTimeResolution(uint32_t resolution);
    
private:
    uint32_t sourceId;
    
    // Random number generation
    std::mt19937 rng;
    std::uniform_int_distribution<uint32_t> energyDist;
    std::uniform_int_distribution<uint32_t> energyShortDist;
    std::uniform_int_distribution<uint32_t> moduleDist;
    std::uniform_int_distribution<uint32_t> channelDist;
    std::uniform_int_distribution<uint32_t> flagsDist;
    
    // Generation parameters
    uint32_t minEnergy;
    uint32_t maxEnergy;
    uint32_t minModule;
    uint32_t maxModule;
    uint32_t minChannel;
    uint32_t maxChannel;
    uint32_t timeResolution;
    
    // Current timestamp
    double currentTimestamp;
    
    // Helper methods
    void UpdateDistributions();
    double GetNextTimestamp();
};

}} // namespace delila::test