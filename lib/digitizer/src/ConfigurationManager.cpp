#include "ConfigurationManager.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Configuration Loading
// ============================================================================

ConfigurationManager::LoadResult ConfigurationManager::LoadFromFile(
    const std::string &filePath)
{
  ClearErrors();

  std::ifstream file(filePath);
  if (!file.is_open()) {
    SetError("Failed to open config file: " + filePath);
    return LoadResult::FileNotFound;
  }

  if (!file.good()) {
    SetError("Config file is not readable: " + filePath);
    return LoadResult::FileNotReadable;
  }

  // Clear existing parameters
  parameters_.clear();
  loadedFilePath_ = filePath;

  std::string line;
  size_t lineNumber = 0;

  while (std::getline(file, line)) {
    ++lineNumber;

    auto result = ParseLine(line, lineNumber);
    if (result != LoadResult::Success) {
      return result;
    }
  }

  // Validate configuration if validator is set
  if (validator_ && !ValidateConfiguration()) {
    return LoadResult::ValidationError;
  }

  return LoadResult::Success;
}

ConfigurationManager::LoadResult ConfigurationManager::LoadFromString(
    const std::string &configString)
{
  ClearErrors();

  // Clear existing parameters
  parameters_.clear();
  loadedFilePath_.clear();

  std::istringstream stream(configString);
  std::string line;
  size_t lineNumber = 0;

  while (std::getline(stream, line)) {
    ++lineNumber;

    auto result = ParseLine(line, lineNumber);
    if (result != LoadResult::Success) {
      return result;
    }
  }

  // Validate configuration if validator is set
  if (validator_ && !ValidateConfiguration()) {
    return LoadResult::ValidationError;
  }

  return LoadResult::Success;
}

void ConfigurationManager::Clear()
{
  parameters_.clear();
  loadedFilePath_.clear();
  ClearErrors();
}

// ============================================================================
// Parameter Access
// ============================================================================

std::string ConfigurationManager::GetParameter(const std::string &key) const
{
  auto it = parameters_.find(key);
  return (it != parameters_.end()) ? it->second : "";
}

std::optional<std::string> ConfigurationManager::GetParameterOptional(
    const std::string &key) const
{
  auto it = parameters_.find(key);
  return (it != parameters_.end()) ? std::make_optional(it->second)
                                   : std::nullopt;
}

bool ConfigurationManager::HasParameter(const std::string &key) const
{
  return parameters_.find(key) != parameters_.end();
}

std::optional<int> ConfigurationManager::GetParameterAsInt(
    const std::string &key) const
{
  auto param = GetParameterOptional(key);
  if (!param) {
    return std::nullopt;
  }

  try {
    return std::stoi(*param);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

std::optional<double> ConfigurationManager::GetParameterAsDouble(
    const std::string &key) const
{
  auto param = GetParameterOptional(key);
  if (!param) {
    return std::nullopt;
  }

  try {
    return std::stod(*param);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

std::optional<bool> ConfigurationManager::GetParameterAsBool(
    const std::string &key) const
{
  auto param = GetParameterOptional(key);
  if (!param) {
    return std::nullopt;
  }

  std::string value = *param;
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);

  if (value == "true" || value == "1" || value == "yes" || value == "on") {
    return true;
  } else if (value == "false" || value == "0" || value == "no" ||
             value == "off") {
    return false;
  }

  return std::nullopt;
}

// ============================================================================
// Parameter Setting
// ============================================================================

void ConfigurationManager::SetParameter(const std::string &key,
                                        const std::string &value)
{
  parameters_[key] = value;
}

// ============================================================================
// Configuration Export
// ============================================================================

std::vector<std::array<std::string, 2>>
ConfigurationManager::GetDigitizerConfig() const
{
  std::vector<std::array<std::string, 2>> config;
  config.reserve(parameters_.size());

  for (const auto &[key, value] : parameters_) {
    config.push_back({key, value});
  }

  return config;
}

std::vector<std::array<std::string, 2>> ConfigurationManager::GetAllParameters()
    const
{
  return GetDigitizerConfig();
}

std::map<std::string, std::string> ConfigurationManager::GetParameterMap() const
{
  return parameters_;
}

// ============================================================================
// Validation
// ============================================================================

void ConfigurationManager::SetValidator(ParameterValidator validator)
{
  validator_ = std::move(validator);
}

bool ConfigurationManager::ValidateConfiguration() const
{
  if (!validator_) {
    return true;
  }

  for (const auto &[key, value] : parameters_) {
    if (!validator_(key, value)) {
      return false;
    }
  }

  return true;
}

std::vector<std::string> ConfigurationManager::GetValidationErrors() const
{
  std::vector<std::string> errors;

  if (!validator_) {
    return errors;
  }

  for (const auto &[key, value] : parameters_) {
    if (!validator_(key, value)) {
      errors.push_back("Invalid parameter: " + key + " = " + value);
    }
  }

  return errors;
}

// ============================================================================
// Information
// ============================================================================

size_t ConfigurationManager::GetParameterCount() const
{
  return parameters_.size();
}

std::vector<std::string> ConfigurationManager::GetParameterKeys() const
{
  std::vector<std::string> keys;
  keys.reserve(parameters_.size());

  for (const auto &[key, value] : parameters_) {
    keys.push_back(key);
  }

  return keys;
}

bool ConfigurationManager::IsEmpty() const { return parameters_.empty(); }

// ============================================================================
// Error Handling
// ============================================================================

std::string ConfigurationManager::GetLastError() const { return lastError_; }

void ConfigurationManager::ClearErrors() { lastError_.clear(); }

void ConfigurationManager::SetError(const std::string &error)
{
  lastError_ = error;
  std::cerr << "ConfigurationManager Error: " << error << std::endl;
}

// ============================================================================
// Helper Methods
// ============================================================================

ConfigurationManager::LoadResult ConfigurationManager::ParseLine(
    const std::string &line, size_t lineNumber)
{
  // Skip empty lines and comments
  if (IsEmptyLine(line) || IsComment(line)) {
    return LoadResult::Success;
  }

  auto [key, value] = SplitKeyValue(line);

  if (key.empty()) {
    SetError("Invalid format at line " + std::to_string(lineNumber) + ": " +
             line);
    return LoadResult::ParseError;
  }

  parameters_[key] = value;
  return LoadResult::Success;
}

std::pair<std::string, std::string> ConfigurationManager::SplitKeyValue(
    const std::string &line) const
{
  // First remove any comments from the line
  std::string cleanLine = line;
  size_t commentPos = cleanLine.find_first_of("#;");
  if (commentPos != std::string::npos) {
    cleanLine = cleanLine.substr(0, commentPos);
  }

  // Find first space or tab to split key and value
  size_t spacePos = cleanLine.find_first_of(" \t");
  if (spacePos == std::string::npos) {
    return {"", ""};
  }

  std::string key = TrimWhitespace(cleanLine.substr(0, spacePos));
  std::string value = TrimWhitespace(cleanLine.substr(spacePos + 1));

  return {key, value};
}

std::string ConfigurationManager::TrimWhitespace(const std::string &str) const
{
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

bool ConfigurationManager::IsComment(const std::string &line) const
{
  std::string trimmed = TrimWhitespace(line);
  return !trimmed.empty() && (trimmed[0] == '#' || trimmed[0] == ';');
}

bool ConfigurationManager::IsEmptyLine(const std::string &line) const
{
  return TrimWhitespace(line).empty();
}

}  // namespace Digitizer
}  // namespace DELILA