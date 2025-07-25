/**
 * @file Platform.cpp
 * @brief Platform-specific utilities implementation
 */

#include "delila/net/utils/Platform.hpp"

namespace DELILA::Net {

uint64_t getCurrentTimestampNs() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto ns = duration_cast<nanoseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(ns.count());
}

uint64_t getUnixTimestampNs() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ns = duration_cast<nanoseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(ns.count());
}

namespace Platform {

bool isLittleEndian() {
    constexpr uint32_t test_value = 0x12345678;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&test_value);
    return bytes[0] == 0x78; // Little-endian: least significant byte first
}

const char* getPlatformName() {
#if defined(DELILA_PLATFORM_LINUX)
    return "Linux";
#elif defined(DELILA_PLATFORM_MACOS)
    return "macOS";
#else
    return "Unknown";
#endif
}

} // namespace Platform

} // namespace DELILA::Net