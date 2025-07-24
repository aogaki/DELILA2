#pragma once

#include "Common.hpp"
#include <set>
#include <atomic>
#include <mutex>
#include <vector>

namespace delila {
namespace test {

class SequenceValidator {
public:
    // Constructor
    SequenceValidator();
    
    // Destructor
    ~SequenceValidator();
    
    // Check sequence number
    bool CheckSequence(uint64_t sequenceNumber, uint32_t sourceId);
    
    // Get validation statistics
    struct ValidationStats {
        uint64_t totalSequences = 0;
        uint64_t duplicateSequences = 0;
        uint64_t outOfOrderSequences = 0;
        uint64_t missingSequences = 0;
        uint64_t expectedNextSequence = 1;
        uint64_t lastReceivedSequence = 0;
    };
    
    ValidationStats GetStats() const;
    
    // Reset validator
    void Reset();
    
    // Check for missing sequences
    std::vector<uint64_t> GetMissingSequences() const;
    
private:
    mutable std::mutex mutex;
    std::set<uint64_t> receivedSequences;
    std::atomic<uint64_t> totalSequences;
    std::atomic<uint64_t> duplicateSequences;
    std::atomic<uint64_t> outOfOrderSequences;
    std::atomic<uint64_t> expectedNextSequence;
    std::atomic<uint64_t> lastReceivedSequence;
};

}} // namespace delila::test