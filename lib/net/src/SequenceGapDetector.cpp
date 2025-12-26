/**
 * @file SequenceGapDetector.cpp
 * @brief Implementation of SequenceGapDetector
 */

#include "SequenceGapDetector.hpp"

namespace DELILA {
namespace Net {

SequenceGapDetector::Result SequenceGapDetector::Check(uint64_t sequence)
{
    // First message - set expected and return Ok
    if (!expected_sequence_.has_value()) {
        expected_sequence_ = sequence + 1;
        return Result::Ok;
    }

    uint64_t expected = expected_sequence_.value();

    // Normal case: sequence matches expected
    if (sequence == expected) {
        expected_sequence_ = sequence + 1;
        return Result::Ok;
    }

    // Gap detected: sequence is ahead of expected
    if (sequence > expected) {
        GapInfo gap;
        gap.expected = expected;
        gap.received = sequence;
        gap.dropped_count = sequence - expected;

        last_gap_ = gap;
        gap_count_++;

        // Update expected to continue from received sequence
        expected_sequence_ = sequence + 1;
        return Result::Gap;
    }

    // Backwards sequence: should not happen in normal operation
    return Result::BackwardsSequence;
}

void SequenceGapDetector::Reset()
{
    expected_sequence_.reset();
    gap_count_ = 0;
    last_gap_.reset();
}

bool SequenceGapDetector::HasExpectedSequence() const
{
    return expected_sequence_.has_value();
}

uint64_t SequenceGapDetector::GetGapCount() const
{
    return gap_count_;
}

std::optional<SequenceGapDetector::GapInfo> SequenceGapDetector::GetLastGap() const
{
    return last_gap_;
}

}  // namespace Net
}  // namespace DELILA
