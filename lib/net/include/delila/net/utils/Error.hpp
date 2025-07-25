#pragma once

#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <chrono>

namespace DELILA::Net {

/**
 * @brief Error information structure for all library operations
 * 
 * Contains error code, descriptive message, timestamp, and optional context.
 * Includes stack trace information in debug builds for debugging assistance.
 */
struct Error {
    /**
     * @brief Error codes for all possible error conditions
     */
    enum Code {
        SUCCESS = 0,                // Operation successful
        INVALID_MESSAGE,            // Message format is invalid
        COMPRESSION_FAILED,         // LZ4 compression/decompression failed  
        CHECKSUM_MISMATCH,          // xxHash32 checksum validation failed
        INVALID_HEADER,             // Binary header is malformed
        VERSION_MISMATCH,           // Unsupported protocol version
        SYSTEM_ERROR,               // Platform-specific system call failed
        INVALID_CONFIG,             // Configuration parameters are invalid
        INVALID_DATA,               // Input data is invalid or corrupted
        INVALID_FORMAT,             // Data format is invalid
        MEMORY_ALLOCATION,          // Memory allocation failed
        SERIALIZATION_ERROR,        // Error during serialization
        DESERIALIZATION_ERROR,      // Error during deserialization
    };
    
    Code code;                      // Error code
    std::string message;            // Human-readable error description
    std::string timestamp;          // Human-readable timestamp (e.g., "2025-07-25 14:30:21")
    std::optional<int> system_errno; // System errno for system call failures
    
#ifndef NDEBUG
    std::vector<std::string> stack_trace; // Stack trace in debug builds only
#endif
    
    /**
     * @brief Construct error with current timestamp
     * @param c Error code
     * @param msg Error message
     * @param errno_val System errno value (0 if not applicable)
     */
    Error(Code c, const std::string& msg, int errno_val = 0);
    
    /**
     * @brief Get current timestamp in human-readable format
     * @return Formatted timestamp string (YYYY-MM-DD HH:MM:SS)
     */
    static std::string getCurrentTimestamp();
    
private:
    void captureStackTrace();
};

/**
 * @brief Result type for error handling without exceptions
 * 
 * Contains either a successful result of type T or an Error.
 * Inspired by Rust's Result<T, E> pattern.
 */
template<typename T>
using Result = std::variant<T, Error>;

/**
 * @brief Helper type for void returns that can fail  
 */
using Status = Result<std::monostate>;

/**
 * @brief Check if Result contains a successful value
 * @param result Result to check
 * @return true if result contains value, false if error
 */
template<typename T>
bool isOk(const Result<T>& result) {
    return std::holds_alternative<T>(result);
}

/**
 * @brief Extract successful value from Result
 * @param result Result containing value
 * @return Reference to the contained value
 * @warning Only call if isOk(result) returns true
 */
template<typename T>
const T& getValue(const Result<T>& result) {
    return std::get<T>(result);
}

/**
 * @brief Extract error from Result
 * @param result Result containing error
 * @return Reference to the contained error
 * @warning Only call if isOk(result) returns false
 */
template<typename T>
const Error& getError(const Result<T>& result) {
    return std::get<Error>(result);
}

/**
 * @brief Create successful Result
 * @param value Value to wrap in Result
 * @return Result containing the value
 */
template<typename T>
Result<T> Ok(T&& value) {
    return Result<T>{std::forward<T>(value)};
}

/**
 * @brief Create error Result
 * @param error Error to wrap in Result
 * @return Result containing the error
 */
template<typename T>
Result<T> Err(Error&& error) {
    return Result<T>{std::forward<Error>(error)};
}

} // namespace DELILA::Net