#ifndef DECODERLOGGER_HPP
#define DECODERLOGGER_HPP

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Decoder Result Enum
// ============================================================================
enum class DecoderResult {
  Success,
  InvalidHeader,
  InsufficientData,
  CorruptedData,
  OutOfBounds,
  InvalidChannelPair,
  InvalidWaveformSize,
  TimestampError,
  UnknownDataType
};

// ============================================================================
// Log Level Enum
// ============================================================================
enum class LogLevel { Error, Warning, Info, Debug };

// ============================================================================
// Decoder Logger Class
// ============================================================================
class DecoderLogger
{
 public:
  /**
   * @brief Set the global log level
   * @param level Minimum level to log
   */
  static void SetLogLevel(LogLevel level) { sLogLevel = level; }

  /**
   * @brief Enable/disable debug output
   * @param enable True to enable debug logging
   */
  static void SetDebugEnabled(bool enable) { sDebugEnabled = enable; }

  /**
   * @brief Log an error message
   * @param context Context where error occurred
   * @param message Error message
   */
  static void LogError(const std::string &context, const std::string &message)
  {
    if (sLogLevel <= LogLevel::Error) {
      std::cerr << "[ERROR] " << context << ": " << message << std::endl;
    }
  }

  /**
   * @brief Log a warning message
   * @param context Context where warning occurred
   * @param message Warning message
   */
  static void LogWarning(const std::string &context, const std::string &message)
  {
    if (sLogLevel <= LogLevel::Warning) {
      std::cerr << "[WARNING] " << context << ": " << message << std::endl;
    }
  }

  /**
   * @brief Log an info message
   * @param context Context for info
   * @param message Info message
   */
  static void LogInfo(const std::string &context, const std::string &message)
  {
    if (sLogLevel <= LogLevel::Info) {
      std::cout << "[INFO] " << context << ": " << message << std::endl;
    }
  }

  /**
   * @brief Log a debug message
   * @param context Context for debug info
   * @param message Debug message
   */
  static void LogDebug(const std::string &context, const std::string &message)
  {
    if (sDebugEnabled && sLogLevel <= LogLevel::Debug) {
      std::cout << "[DEBUG] " << context << ": " << message << std::endl;
    }
  }

  /**
   * @brief Log decoder result with context
   * @param result The decoder result
   * @param context Context information
   * @param details Additional details
   */
  static void LogResult(DecoderResult result, const std::string &context,
                        const std::string &details = "")
  {
    std::string resultStr = ResultToString(result);
    std::string message = resultStr;
    if (!details.empty()) {
      message += " - " + details;
    }

    if (result == DecoderResult::Success) {
      LogDebug(context, message);
    } else {
      LogError(context, message);
    }
  }

  /**
   * @brief Log memory access information
   * @param context Context of memory access
   * @param wordIndex Current word index
   * @param totalWords Total available words
   * @param operation Operation being performed
   */
  static void LogMemoryAccess(const std::string &context, size_t wordIndex,
                              size_t totalWords, const std::string &operation)
  {
    if (sDebugEnabled) {
      std::ostringstream oss;
      oss << operation << " at word " << wordIndex << "/" << totalWords;
      LogDebug(context, oss.str());
    }
  }

  /**
   * @brief Log hex dump of data
   * @param context Context for hex dump
   * @param data Pointer to data
   * @param size Size in bytes
   * @param maxBytes Maximum bytes to dump (0 = all)
   */
  static void LogHexDump(const std::string &context, const uint8_t *data,
                         size_t size, size_t maxBytes = 64)
  {
    if (!sDebugEnabled) return;

    std::ostringstream oss;
    oss << "Hex dump (" << size << " bytes):\n";

    size_t dumpSize = (maxBytes > 0) ? std::min(size, maxBytes) : size;
    for (size_t i = 0; i < dumpSize; i += 16) {
      oss << std::hex << std::setw(8) << std::setfill('0') << i << ": ";

      // Hex bytes
      for (size_t j = 0; j < 16 && i + j < dumpSize; ++j) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i + j]) << " ";
      }

      // Fill remaining space
      for (size_t j = dumpSize - i; j < 16; ++j) {
        oss << "   ";
      }

      oss << " ";

      // ASCII representation
      for (size_t j = 0; j < 16 && i + j < dumpSize; ++j) {
        char c = data[i + j];
        oss << (std::isprint(c) ? c : '.');
      }

      oss << "\n";
    }

    if (dumpSize < size) {
      oss << "... (" << (size - dumpSize) << " more bytes)\n";
    }

    LogDebug(context, oss.str());
  }

  /**
   * @brief Convert DecoderResult to string
   * @param result The result to convert
   * @return String representation
   */
  static std::string ResultToString(DecoderResult result)
  {
    switch (result) {
      case DecoderResult::Success:
        return "Success";
      case DecoderResult::InvalidHeader:
        return "Invalid header";
      case DecoderResult::InsufficientData:
        return "Insufficient data";
      case DecoderResult::CorruptedData:
        return "Corrupted data";
      case DecoderResult::OutOfBounds:
        return "Out of bounds access";
      case DecoderResult::InvalidChannelPair:
        return "Invalid channel pair";
      case DecoderResult::InvalidWaveformSize:
        return "Invalid waveform size";
      case DecoderResult::TimestampError:
        return "Timestamp calculation error";
      case DecoderResult::UnknownDataType:
        return "Unknown data type";
      default:
        return "Unknown result";
    }
  }

 private:
  static LogLevel sLogLevel;
  static bool sDebugEnabled;
};

// Static member initialization
inline LogLevel DecoderLogger::sLogLevel = LogLevel::Warning;
inline bool DecoderLogger::sDebugEnabled = false;

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DECODERLOGGER_HPP