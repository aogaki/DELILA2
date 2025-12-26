/**
 * @file MessageType.hpp
 * @brief Message type definitions for DELILA2 protocol
 *
 * Defines the types of messages that can be sent between components:
 * - Data: Normal event data
 * - Heartbeat: Keep-alive for low-rate detectors
 * - EndOfStream: Graceful stop marker
 */

#pragma once

#include <cstdint>
#include <string>

namespace DELILA {
namespace Net {

/**
 * @brief Type of message in the data stream
 */
enum class MessageType : uint8_t {
    Data = 0,         ///< Normal event data
    Heartbeat = 1,    ///< Keep-alive message (no events)
    EndOfStream = 2   ///< End of run marker
};

/**
 * @brief Convert MessageType to string for logging
 */
inline std::string MessageTypeToString(MessageType type)
{
    switch (type) {
        case MessageType::Data:
            return "Data";
        case MessageType::Heartbeat:
            return "Heartbeat";
        case MessageType::EndOfStream:
            return "EndOfStream";
        default:
            return "Unknown";
    }
}

/**
 * @brief Check if message type is Data
 */
inline bool IsDataMessage(MessageType type)
{
    return type == MessageType::Data;
}

/**
 * @brief Check if message type is Heartbeat
 */
inline bool IsHeartbeat(MessageType type)
{
    return type == MessageType::Heartbeat;
}

/**
 * @brief Check if message type is EndOfStream
 */
inline bool IsEndOfStream(MessageType type)
{
    return type == MessageType::EndOfStream;
}

}  // namespace Net
}  // namespace DELILA
