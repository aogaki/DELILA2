/**
 * @file HeartbeatMonitor.hpp
 * @brief Monitors heartbeat/data reception from multiple sources
 *
 * Used on the receiving side to detect when sources have stopped
 * sending data (connection lost or source crashed).
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace DELILA {
namespace Net {

/**
 * @brief Monitors heartbeat reception from multiple sources
 *
 * Usage:
 *   HeartbeatMonitor monitor(6000ms);  // 6 second timeout
 *   while (running) {
 *       auto msg = receive_message();
 *       monitor.Update(msg.source_id);
 *
 *       auto timedOut = monitor.GetTimedOutSources();
 *       if (!timedOut.empty()) {
 *           // Handle timeout - CRITICAL ERROR
 *       }
 *   }
 */
class HeartbeatMonitor {
public:
    /**
     * @brief Constructor with configurable timeout
     * @param timeout Time after which a source is considered timed out
     */
    explicit HeartbeatMonitor(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(6000))
        : timeout_(timeout)
    {
    }

    /**
     * @brief Update the last-seen time for a source
     * @param source_id Identifier of the source
     */
    void Update(const std::string& source_id)
    {
        last_seen_[source_id] = std::chrono::steady_clock::now();
    }

    /**
     * @brief Check if a specific source has timed out
     * @param source_id Identifier of the source
     * @return true if source exists and has timed out
     */
    bool IsTimedOut(const std::string& source_id) const
    {
        auto it = last_seen_.find(source_id);
        if (it == last_seen_.end()) {
            return false;  // Unknown source is not considered timed out
        }

        auto now = std::chrono::steady_clock::now();
        return (now - it->second) >= timeout_;
    }

    /**
     * @brief Get list of all sources that have timed out
     * @return Vector of timed-out source IDs
     */
    std::vector<std::string> GetTimedOutSources() const
    {
        std::vector<std::string> result;
        auto now = std::chrono::steady_clock::now();

        for (const auto& [source_id, last_time] : last_seen_) {
            if ((now - last_time) >= timeout_) {
                result.push_back(source_id);
            }
        }

        return result;
    }

    /**
     * @brief Remove a source from monitoring
     * @param source_id Identifier of the source to remove
     */
    void Remove(const std::string& source_id)
    {
        last_seen_.erase(source_id);
    }

    /**
     * @brief Clear all sources
     */
    void Clear()
    {
        last_seen_.clear();
    }

    /**
     * @brief Get the current timeout value
     * @return Timeout duration
     */
    std::chrono::milliseconds GetTimeout() const
    {
        return timeout_;
    }

    /**
     * @brief Get the number of sources being monitored
     * @return Number of sources
     */
    size_t GetSourceCount() const
    {
        return last_seen_.size();
    }

private:
    std::chrono::milliseconds timeout_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_seen_;
};

}  // namespace Net
}  // namespace DELILA
