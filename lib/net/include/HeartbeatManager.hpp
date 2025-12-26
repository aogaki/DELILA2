/**
 * @file HeartbeatManager.hpp
 * @brief Manages heartbeat timing for data sources
 *
 * Heartbeats are sent when:
 * 1. No data has been sent for the heartbeat interval
 * 2. Used to prevent timeout detection on slow/idle sources
 * 3. Keeps time-sorting windows moving in low-rate detectors
 */

#pragma once

#include <chrono>

namespace DELILA {
namespace Net {

/**
 * @brief Manages when to send heartbeat messages
 *
 * Usage:
 *   HeartbeatManager heartbeat(100ms);
 *   while (running) {
 *       if (has_data) {
 *           send_data();
 *           heartbeat.MarkSent();  // Reset timer on data send
 *       } else if (heartbeat.IsDue()) {
 *           send_heartbeat();
 *           heartbeat.MarkSent();
 *       }
 *   }
 */
class HeartbeatManager {
public:
    /**
     * @brief Constructor with configurable interval
     * @param interval Time between heartbeats (default 100ms)
     */
    explicit HeartbeatManager(
        std::chrono::milliseconds interval = std::chrono::milliseconds(100))
        : interval_(interval), last_sent_(std::chrono::steady_clock::now())
    {
    }

    /**
     * @brief Check if heartbeat is due
     * @return true if interval has elapsed since last send
     */
    bool IsDue() const
    {
        auto now = std::chrono::steady_clock::now();
        return (now - last_sent_) >= interval_;
    }

    /**
     * @brief Mark that a message (data or heartbeat) was sent
     *
     * Call this after sending any message to reset the timer.
     */
    void MarkSent()
    {
        last_sent_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Set the heartbeat interval
     * @param interval New interval
     */
    void SetInterval(std::chrono::milliseconds interval)
    {
        interval_ = interval;
    }

    /**
     * @brief Get the current heartbeat interval
     * @return Current interval
     */
    std::chrono::milliseconds GetInterval() const
    {
        return interval_;
    }

private:
    std::chrono::milliseconds interval_;
    std::chrono::steady_clock::time_point last_sent_;
};

}  // namespace Net
}  // namespace DELILA
