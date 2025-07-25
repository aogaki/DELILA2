#pragma once

#include <cstdint>
#include <chrono>

// Platform detection and utilities

namespace DELILA::Net {

// Compile-time endianness check
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, 
              "DELILA networking library requires little-endian platform");

/**
 * @brief Get current high-resolution timestamp in nanoseconds
 * @return Nanoseconds since steady clock epoch (for relative timing)
 */
uint64_t getCurrentTimestampNs();

/**
 * @brief Get current Unix timestamp in nanoseconds  
 * @return Nanoseconds since Unix epoch (for absolute timestamps)
 */
uint64_t getUnixTimestampNs();

/**
 * @brief Platform information utilities
 */
namespace Platform {
    
    /**
     * @brief Check if running on little-endian system
     * @return true if little-endian, false otherwise
     */
    bool isLittleEndian();
    
    /**
     * @brief Get platform name
     * @return Platform name string ("Linux", "macOS", etc.)
     */
    const char* getPlatformName();
    
} // namespace Platform

} // namespace DELILA::Net