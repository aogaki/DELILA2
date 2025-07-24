#ifndef CONFIGURATIONMANAGER_HPP
#define CONFIGURATIONMANAGER_HPP

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Configuration management for digitizer parameters
 *
 * This class handles loading and parsing configuration files,
 * providing type-safe parameter access and validation capabilities.
 */
class ConfigurationManager
{
 public:
  // Error codes for better error handling
  enum class LoadResult {
    Success,
    FileNotFound,
    FileNotReadable,
    ParseError,
    ValidationError
  };

  // Parameter validation function type
  using ParameterValidator =
      std::function<bool(const std::string &key, const std::string &value)>;

  // Constructors and Destructor
  ConfigurationManager() = default;
  ~ConfigurationManager() = default;

  // Copy semantics
  ConfigurationManager(const ConfigurationManager &other) = default;
  ConfigurationManager &operator=(const ConfigurationManager &other) = default;

  // Move semantics
  ConfigurationManager(ConfigurationManager &&other) noexcept = default;
  ConfigurationManager &operator=(ConfigurationManager &&other) noexcept =
      default;

  // Configuration loading
  LoadResult LoadFromFile(const std::string &filePath);
  LoadResult LoadFromString(const std::string &configString);
  void Clear();

  // Parameter access
  std::string GetParameter(const std::string &key) const;
  std::optional<std::string> GetParameterOptional(const std::string &key) const;
  bool HasParameter(const std::string &key) const;

  // Type-safe parameter getters
  template <typename T>
  std::optional<T> GetParameterAs(const std::string &key) const;

  std::optional<int> GetParameterAsInt(const std::string &key) const;
  std::optional<double> GetParameterAsDouble(const std::string &key) const;
  std::optional<bool> GetParameterAsBool(const std::string &key) const;

  // Parameter setting (for programmatic configuration)
  void SetParameter(const std::string &key, const std::string &value);
  template <typename T>
  void SetParameterAs(const std::string &key, const T &value);

  // Configuration export
  std::vector<std::array<std::string, 2>> GetDigitizerConfig() const;
  std::vector<std::array<std::string, 2>> GetAllParameters() const;
  std::map<std::string, std::string> GetParameterMap() const;

  // Validation
  void SetValidator(ParameterValidator validator);
  bool ValidateConfiguration() const;
  std::vector<std::string> GetValidationErrors() const;

  // Information
  size_t GetParameterCount() const;
  std::vector<std::string> GetParameterKeys() const;
  bool IsEmpty() const;

  // Error handling
  std::string GetLastError() const;
  void ClearErrors();

  // File information
  std::string GetLoadedFilePath() const { return loadedFilePath_; }
  bool IsFileLoaded() const { return !loadedFilePath_.empty(); }

 private:
  // Data members
  std::map<std::string, std::string> parameters_;
  std::string loadedFilePath_;
  std::string lastError_;
  ParameterValidator validator_;

  // Helper methods
  LoadResult ParseLine(const std::string &line, size_t lineNumber);
  std::pair<std::string, std::string> SplitKeyValue(
      const std::string &line) const;
  std::string TrimWhitespace(const std::string &str) const;
  bool IsComment(const std::string &line) const;
  bool IsEmptyLine(const std::string &line) const;
  void SetError(const std::string &error);
};

// Template implementations
template <typename T>
std::optional<T> ConfigurationManager::GetParameterAs(
    const std::string &key) const
{
  static_assert(std::is_arithmetic_v<T>,
                "GetParameterAs only supports arithmetic types");
  // Implementation would be in .cpp file for most types
  return std::nullopt;
}

template <typename T>
void ConfigurationManager::SetParameterAs(const std::string &key,
                                          const T &value)
{
  SetParameter(key, std::to_string(value));
}

// Specialization for string
template <>
inline void ConfigurationManager::SetParameterAs<std::string>(
    const std::string &key, const std::string &value)
{
  SetParameter(key, value);
}

}  // namespace Digitizer
}  // namespace DELILA

#endif  // CONFIGURATIONMANAGER_HPP