/**
 * @file SequenceGapDetector.hpp
 * @brief Detects gaps in sequence numbers to prevent silent data drops
 *
 * ZeroMQ PUB sockets silently drop messages when subscribers are slow.
 * This class detects such drops by tracking sequence numbers.
 */

#pragma once

#include <cstdint>
#include <optional>

namespace DELILA {
namespace Net {

/**
 * @brief Detects gaps in sequence numbers
 *
 * Usage:
 *   SequenceGapDetector detector;
 *   for each received message:
 *       auto result = detector.Check(message.sequence_number);
 *       if (result == Result::Gap) {
 *           // Handle data loss - CRITICAL ERROR in production
 *       }
 */
class SequenceGapDetector {
public:
    /**
     * @brief Information about a detected gap
     */
    struct GapInfo {
        uint64_t expected;      ///< Expected sequence number
        uint64_t received;      ///< Actually received sequence number
        uint64_t dropped_count; ///< Number of dropped messages
    };

    /**
     * @brief Result of sequence check
     */
    enum class Result {
        Ok,                ///< Sequence is as expected
        Gap,               ///< Gap detected - messages were dropped
        BackwardsSequence  ///< Sequence went backwards - should not happen
    };

    SequenceGapDetector() = default;

    /**
     * @brief Check a sequence number
     * @param sequence The sequence number to check
     * @return Result indicating if sequence is ok or gap detected
     */
    Result Check(uint64_t sequence);

    /**
     * @brief Reset detector state (call at start of new run)
     */
    void Reset();

    /**
     * @brief Check if detector has an expected sequence set
     */
    bool HasExpectedSequence() const;

    /**
     * @brief Get total number of gaps detected
     */
    uint64_t GetGapCount() const;

    /**
     * @brief Get information about the last detected gap
     * @return GapInfo if a gap was detected, nullopt otherwise
     */
    std::optional<GapInfo> GetLastGap() const;

private:
    std::optional<uint64_t> expected_sequence_;
    uint64_t gap_count_ = 0;
    std::optional<GapInfo> last_gap_;
};

}  // namespace Net
}  // namespace DELILA
