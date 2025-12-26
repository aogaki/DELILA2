/**
 * @file EOSTracker.hpp
 * @brief Tracks End-Of-Stream messages from multiple sources
 *
 * Used for graceful stop: wait for all sources to send EOS
 * before completing the run and closing files.
 */

#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace DELILA {
namespace Net {

/**
 * @brief Tracks EOS reception from multiple data sources
 *
 * Usage:
 *   EOSTracker tracker;
 *
 *   // At run start: register all expected sources
 *   tracker.RegisterSource("source1");
 *   tracker.RegisterSource("source2");
 *
 *   // During run: mark EOS as received
 *   tracker.ReceiveEOS("source1");
 *
 *   // Check completion
 *   if (tracker.AllReceived()) {
 *       // Safe to close files
 *   }
 */
class EOSTracker {
public:
    /**
     * @brief Register a source that is expected to send EOS
     * @param source_id Identifier of the source
     */
    void RegisterSource(const std::string& source_id)
    {
        expected_sources_.insert(source_id);
    }

    /**
     * @brief Unregister a source (e.g., if it disconnects before run)
     * @param source_id Identifier of the source
     */
    void UnregisterSource(const std::string& source_id)
    {
        expected_sources_.erase(source_id);
        received_sources_.erase(source_id);
    }

    /**
     * @brief Mark that EOS was received from a source
     * @param source_id Identifier of the source
     */
    void ReceiveEOS(const std::string& source_id)
    {
        // Only count if source is registered
        if (expected_sources_.find(source_id) != expected_sources_.end()) {
            received_sources_.insert(source_id);
        }
    }

    /**
     * @brief Check if all expected sources have sent EOS
     * @return true if all EOS received (or no sources expected)
     */
    bool AllReceived() const
    {
        return received_sources_.size() == expected_sources_.size();
    }

    /**
     * @brief Get list of sources that haven't sent EOS yet
     * @return Vector of pending source IDs
     */
    std::vector<std::string> GetPendingSources() const
    {
        std::vector<std::string> pending;

        for (const auto& source_id : expected_sources_) {
            if (received_sources_.find(source_id) == received_sources_.end()) {
                pending.push_back(source_id);
            }
        }

        return pending;
    }

    /**
     * @brief Check if a source has sent EOS
     * @param source_id Identifier of the source
     * @return true if EOS received from this source
     */
    bool HasReceivedEOS(const std::string& source_id) const
    {
        return received_sources_.find(source_id) != received_sources_.end();
    }

    /**
     * @brief Check if a source is registered
     * @param source_id Identifier of the source
     * @return true if source is registered
     */
    bool IsRegistered(const std::string& source_id) const
    {
        return expected_sources_.find(source_id) != expected_sources_.end();
    }

    /**
     * @brief Reset tracker for new run
     */
    void Reset()
    {
        expected_sources_.clear();
        received_sources_.clear();
    }

    /**
     * @brief Get number of expected sources
     */
    size_t GetExpectedCount() const
    {
        return expected_sources_.size();
    }

    /**
     * @brief Get number of sources that have sent EOS
     */
    size_t GetReceivedCount() const
    {
        return received_sources_.size();
    }

private:
    std::unordered_set<std::string> expected_sources_;
    std::unordered_set<std::string> received_sources_;
};

}  // namespace Net
}  // namespace DELILA
