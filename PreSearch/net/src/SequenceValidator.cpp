#include "SequenceValidator.hpp"

namespace delila {
namespace test {

SequenceValidator::SequenceValidator() 
    : totalSequences(0), duplicateSequences(0), outOfOrderSequences(0),
      expectedNextSequence(1), lastReceivedSequence(0) {
}

SequenceValidator::~SequenceValidator() = default;

bool SequenceValidator::CheckSequence(uint64_t sequenceNumber, uint32_t sourceId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    totalSequences.fetch_add(1);
    
    // Check for duplicate
    if (receivedSequences.find(sequenceNumber) != receivedSequences.end()) {
        duplicateSequences.fetch_add(1);
        return false;
    }
    
    // Add to received set
    receivedSequences.insert(sequenceNumber);
    
    // Check if out of order
    uint64_t expected = expectedNextSequence.load();
    if (sequenceNumber < expected) {
        outOfOrderSequences.fetch_add(1);
    }
    
    // Update expected next sequence
    if (sequenceNumber >= expected) {
        expectedNextSequence.store(sequenceNumber + 1);
    }
    
    // Update last received
    uint64_t lastReceived = lastReceivedSequence.load();
    if (sequenceNumber > lastReceived) {
        lastReceivedSequence.store(sequenceNumber);
    }
    
    return true;
}

SequenceValidator::ValidationStats SequenceValidator::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    ValidationStats stats;
    stats.totalSequences = totalSequences.load();
    stats.duplicateSequences = duplicateSequences.load();
    stats.outOfOrderSequences = outOfOrderSequences.load();
    stats.expectedNextSequence = expectedNextSequence.load();
    stats.lastReceivedSequence = lastReceivedSequence.load();
    
    // Calculate missing sequences
    uint64_t expectedTotal = stats.lastReceivedSequence;
    if (expectedTotal > 0) {
        stats.missingSequences = expectedTotal - receivedSequences.size();
    }
    
    return stats;
}

void SequenceValidator::Reset() {
    std::lock_guard<std::mutex> lock(mutex);
    
    receivedSequences.clear();
    totalSequences.store(0);
    duplicateSequences.store(0);
    outOfOrderSequences.store(0);
    expectedNextSequence.store(1);
    lastReceivedSequence.store(0);
}

std::vector<uint64_t> SequenceValidator::GetMissingSequences() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::vector<uint64_t> missing;
    uint64_t lastReceived = lastReceivedSequence.load();
    
    for (uint64_t seq = 1; seq <= lastReceived; ++seq) {
        if (receivedSequences.find(seq) == receivedSequences.end()) {
            missing.push_back(seq);
        }
    }
    
    return missing;
}

}} // namespace delila::test