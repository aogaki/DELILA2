/**
 * @file Error.cpp
 * @brief Implementation of Error handling and Result pattern
 */

#include "delila/net/utils/Error.hpp"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>

#ifndef NDEBUG
#include <execinfo.h>  // For backtrace (Linux/macOS)
#include <cstdlib>
#include <memory>
#endif

namespace DELILA::Net {

Error::Error(Code c, const std::string& msg, int errno_val)
    : code(c), message(msg), timestamp(getCurrentTimestamp()) {
    
    // Add errno information if provided
    if (errno_val != 0) {
        system_errno = errno_val;
        // Append errno description to message
        message += " (errno: " + std::to_string(errno_val) + " - " + std::strerror(errno_val) + ")";
    }
    
#ifndef NDEBUG
    // Capture stack trace in debug builds
    captureStackTrace();
#endif
}

std::string Error::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto tm_now = *std::localtime(&time_t_now);
    
    // Format: "YYYY-MM-DD HH:MM:SS"
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Error::captureStackTrace() {
#ifndef NDEBUG
    // Capture stack trace using backtrace (POSIX)
    constexpr int MAX_FRAMES = 16;
    void* buffer[MAX_FRAMES];
    
    int frame_count = backtrace(buffer, MAX_FRAMES);
    if (frame_count > 0) {
        char** symbols = backtrace_symbols(buffer, frame_count);
        if (symbols) {
            stack_trace.reserve(frame_count);
            for (int i = 0; i < frame_count; ++i) {
                stack_trace.emplace_back(symbols[i]);
            }
            std::free(symbols);
        }
    }
    
    // If backtrace failed or no frames, add a placeholder
    if (stack_trace.empty()) {
        stack_trace.emplace_back("Stack trace capture not available");
    }
#endif
}

} // namespace DELILA::Net