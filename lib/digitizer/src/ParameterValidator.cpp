#include "ParameterValidator.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Constructor
// ============================================================================

ParameterValidator::ParameterValidator(const nlohmann::json &deviceTree)
    : deviceTree_(deviceTree)
{
}

// ============================================================================
// Main Validation Methods
// ============================================================================

ParameterValidator::ValidationSummary ParameterValidator::ValidateParameters(
    const std::vector<std::array<std::string, 2>> &config)
{
  ValidationSummary summary;

  if (!silentMode_) {
    std::cout << "\n=== Parameter Validation ===" << std::endl;
  }

  for (const auto &configPair : config) {
    // Only validate parameters that start with '/' (CAEN paths)
    if (!configPair[0].empty() && configPair[0][0] == '/') {
      const std::string &paramPath = configPair[0];
      const std::string &paramValue = configPair[1];

      // Skip ignored parameters
      if (IsIgnored(paramPath)) {
        continue;
      }

      ValidationResult result;

      // Check if this is a range parameter
      if (paramPath.find("..") != std::string::npos) {
        // For range parameters, validate one channel and show the range
        auto expandedPaths = ExpandChannelRange(paramPath);
        if (!expandedPaths.empty()) {
          result = ValidateSingleParameter(expandedPaths[0], paramValue);
          result.paramPath = paramPath;  // Use original range path for display
        } else {
          result = ValidationResult(false, paramPath, paramValue,
                                    "Failed to expand channel range");
        }
      } else {
        // Single parameter, validate normally
        result = ValidateSingleParameter(paramPath, paramValue);
      }

      // Update summary statistics
      summary.totalParameters++;
      if (result.isValid) {
        summary.validParameters++;
      } else {
        summary.invalidParameters++;
      }

      if (!result.warningMessage.empty()) {
        summary.warningParameters++;
      }

      summary.results.push_back(result);

      // Print result if not in silent mode
      if (!silentMode_) {
        std::cout << FormatValidationMessage(result) << std::endl;
      }
    }
  }

  if (!silentMode_) {
    PrintValidationReport(summary);
  }

  return summary;
}

ParameterValidator::ValidationResult ParameterValidator::ValidateParameter(
    const std::string &paramPath, const std::string &value)
{
  return ValidateSingleParameter(paramPath, value);
}

ParameterValidator::ValidationResult
ParameterValidator::ValidateSingleParameter(const std::string &paramPath,
                                            const std::string &value)
{
  // Check custom validators first
  for (const auto &[pattern, validator] : customValidators_) {
    if (MatchesPattern(paramPath, pattern)) {
      auto paramDef = GetParameterDefinition(paramPath);
      return validator(paramPath, value, paramDef);
    }
  }

  // Get parameter definition from device tree
  nlohmann::json paramDef = GetParameterDefinition(paramPath);

  if (paramDef.empty()) {
    if (allowUnknownParameters_) {
      return ValidationResult(true, paramPath, value, "",
                              "Parameter not found in device tree");
    } else {
      return ValidationResult(false, paramPath, value,
                              "Parameter not found in device tree");
    }
  }

  // Validate based on parameter definition
  return ValidateParameterValue(paramDef, paramPath, value);
}

// ============================================================================
// Configuration Methods
// ============================================================================

void ParameterValidator::AddCustomValidator(const std::string &paramPattern,
                                            CustomValidator validator)
{
  customValidators_[paramPattern] = std::move(validator);
}

void ParameterValidator::RemoveCustomValidator(const std::string &paramPattern)
{
  customValidators_.erase(paramPattern);
}

void ParameterValidator::ClearCustomValidators() { customValidators_.clear(); }

void ParameterValidator::AddIgnorePattern(const std::string &pattern)
{
  ignorePatterns_.insert(pattern);
}

void ParameterValidator::RemoveIgnorePattern(const std::string &pattern)
{
  ignorePatterns_.erase(pattern);
}

bool ParameterValidator::IsIgnored(const std::string &paramPath) const
{
  for (const auto &pattern : ignorePatterns_) {
    if (MatchesPattern(paramPath, pattern)) {
      return true;
    }
  }
  return false;
}

// ============================================================================
// Parameter Information
// ============================================================================

ParameterValidator::ParameterType ParameterValidator::GetParameterType(
    const std::string &paramPath) const
{
  auto paramDef = GetParameterDefinition(paramPath);
  return ParseParameterType(paramDef);
}

std::optional<std::string> ParameterValidator::GetParameterDescription(
    const std::string &paramPath) const
{
  auto paramDef = GetParameterDefinition(paramPath);
  if (paramDef.contains("description") &&
      paramDef["description"].contains("value")) {
    return paramDef["description"]["value"].get<std::string>();
  }
  return std::nullopt;
}

bool ParameterValidator::IsParameterSupported(
    const std::string &paramPath) const
{
  return !GetParameterDefinition(paramPath).empty();
}

// ============================================================================
// Parameter Path Processing
// ============================================================================

std::vector<std::string> ParameterValidator::ExpandChannelRange(
    const std::string &configPath) const
{
  std::vector<std::string> expandedPaths;

  size_t dotPos = configPath.find("..");
  if (dotPos == std::string::npos) {
    expandedPaths.push_back(configPath);
    return expandedPaths;
  }

  size_t chPos = configPath.find("/ch/");
  if (chPos == std::string::npos) {
    expandedPaths.push_back(configPath);
    return expandedPaths;
  }

  size_t rangeStart = chPos + 4;
  size_t rangeEnd = configPath.find("/", rangeStart);

  if (rangeEnd == std::string::npos) {
    expandedPaths.push_back(configPath);
    return expandedPaths;
  }

  std::string rangeStr = configPath.substr(rangeStart, rangeEnd - rangeStart);
  std::string prefix = configPath.substr(0, rangeStart);
  std::string suffix = configPath.substr(rangeEnd);

  size_t dotDotPos = rangeStr.find("..");
  if (dotDotPos == std::string::npos) {
    expandedPaths.push_back(configPath);
    return expandedPaths;
  }

  try {
    int startCh = std::stoi(rangeStr.substr(0, dotDotPos));
    int endCh = std::stoi(rangeStr.substr(dotDotPos + 2));

    if (startCh > endCh || startCh < 0 || endCh > 1000) {
      expandedPaths.push_back(configPath);
      return expandedPaths;
    }

    expandedPaths.reserve(endCh - startCh + 1);
    for (int ch = startCh; ch <= endCh; ++ch) {
      expandedPaths.push_back(prefix + std::to_string(ch) + suffix);
    }
  } catch (const std::exception &) {
    expandedPaths.push_back(configPath);
  }

  return expandedPaths;
}

std::string ParameterValidator::MapConfigToDeviceTree(
    const std::string &configParam) const
{
  size_t lastSlash = configParam.find_last_of('/');
  if (lastSlash == std::string::npos) {
    return "";
  }

  std::string paramName = configParam.substr(lastSlash + 1);
  std::string lowerParam = paramName;
  std::transform(lowerParam.begin(), lowerParam.end(), lowerParam.begin(),
                 ::tolower);

  return lowerParam;
}

bool ParameterValidator::MatchesPattern(const std::string &path,
                                        const std::string &pattern) const
{
  try {
    std::regex regex(pattern);
    return std::regex_match(path, regex);
  } catch (const std::exception &) {
    return path.find(pattern) != std::string::npos;
  }
}

// ============================================================================
// Validation Engine
// ============================================================================

ParameterValidator::ValidationResult ParameterValidator::ValidateParameterValue(
    const nlohmann::json &paramDef, const std::string &paramPath,
    const std::string &value) const
{
  ParameterType type = ParseParameterType(paramDef);

  switch (type) {
    case ParameterType::Number:
      return ValidateNumberParameter(paramDef, paramPath, value);
    case ParameterType::Integer:
      return ValidateIntegerParameter(paramDef, paramPath, value);
    case ParameterType::String:
      return ValidateStringParameter(paramDef, paramPath, value);
    case ParameterType::Boolean:
      return ValidateBooleanParameter(paramDef, paramPath, value);
    case ParameterType::Enum:
      return ValidateEnumParameter(paramDef, paramPath, value);
    default:
      return ValidationResult(true, paramPath, value, "",
                              "Unknown parameter type");
  }
}

ParameterValidator::ValidationResult
ParameterValidator::ValidateNumberParameter(const nlohmann::json &paramDef,
                                            const std::string &paramPath,
                                            const std::string &value) const
{
  try {
    double numValue = std::stod(value);

    if (paramDef.contains("minvalue") &&
        paramDef["minvalue"].contains("value")) {
      double minVal =
          std::stod(paramDef["minvalue"]["value"].get<std::string>());
      if (numValue < minVal) {
        return ValidationResult(
            false, paramPath, value,
            "Value " + value + " below minimum: " +
                paramDef["minvalue"]["value"].get<std::string>());
      }
    }

    if (paramDef.contains("maxvalue") &&
        paramDef["maxvalue"].contains("value")) {
      double maxVal =
          std::stod(paramDef["maxvalue"]["value"].get<std::string>());
      if (numValue > maxVal) {
        return ValidationResult(
            false, paramPath, value,
            "Value " + value + " above maximum: " +
                paramDef["maxvalue"]["value"].get<std::string>());
      }
    }

    return ValidationResult(true, paramPath, value);
  } catch (const std::exception &) {
    return ValidationResult(false, paramPath, value, "Invalid number format");
  }
}

ParameterValidator::ValidationResult
ParameterValidator::ValidateIntegerParameter(const nlohmann::json &paramDef,
                                             const std::string &paramPath,
                                             const std::string &value) const
{
  try {
    int intValue = std::stoi(value);

    if (paramDef.contains("minvalue") &&
        paramDef["minvalue"].contains("value")) {
      int minVal = std::stoi(paramDef["minvalue"]["value"].get<std::string>());
      if (intValue < minVal) {
        return ValidationResult(
            false, paramPath, value,
            "Value " + value + " below minimum: " +
                paramDef["minvalue"]["value"].get<std::string>());
      }
    }

    if (paramDef.contains("maxvalue") &&
        paramDef["maxvalue"].contains("value")) {
      int maxVal = std::stoi(paramDef["maxvalue"]["value"].get<std::string>());
      if (intValue > maxVal) {
        return ValidationResult(
            false, paramPath, value,
            "Value " + value + " above maximum: " +
                paramDef["maxvalue"]["value"].get<std::string>());
      }
    }

    return ValidationResult(true, paramPath, value);
  } catch (const std::exception &) {
    return ValidationResult(false, paramPath, value, "Invalid integer format");
  }
}

ParameterValidator::ValidationResult
ParameterValidator::ValidateStringParameter(const nlohmann::json &paramDef,
                                            const std::string &paramPath,
                                            const std::string &value) const
{
  // Basic string validation - can be extended for allowed values
  return ValidationResult(true, paramPath, value);
}

ParameterValidator::ValidationResult
ParameterValidator::ValidateBooleanParameter(const nlohmann::json &paramDef,
                                             const std::string &paramPath,
                                             const std::string &value) const
{
  std::string lowerValue = value;
  std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                 ::tolower);

  if (lowerValue == "true" || lowerValue == "false" || lowerValue == "1" ||
      lowerValue == "0" || lowerValue == "yes" || lowerValue == "no") {
    return ValidationResult(true, paramPath, value);
  }

  return ValidationResult(false, paramPath, value, "Invalid boolean value");
}

ParameterValidator::ValidationResult ParameterValidator::ValidateEnumParameter(
    const nlohmann::json &paramDef, const std::string &paramPath,
    const std::string &value) const
{
  // Enum validation would check against allowed values
  return ValidationResult(true, paramPath, value);
}

// ============================================================================
// Device Tree Navigation
// ============================================================================

nlohmann::json ParameterValidator::GetParameterDefinition(
    const std::string &paramPath) const
{
  if (paramPath.find("/ch/") != std::string::npos) {
    size_t chStart = paramPath.find("/ch/") + 4;
    size_t chEnd = paramPath.find("/", chStart);

    if (chEnd == std::string::npos) return nlohmann::json();

    std::string channel = paramPath.substr(chStart, chEnd - chStart);
    std::string paramName = MapConfigToDeviceTree(paramPath);

    return GetChannelParameterDefinition(channel, paramName);
  } else if (paramPath.find("/par/") != std::string::npos) {
    std::string paramName = MapConfigToDeviceTree(paramPath);
    return GetRootParameterDefinition(paramName);
  }

  return nlohmann::json();
}

nlohmann::json ParameterValidator::GetChannelParameterDefinition(
    const std::string &channel, const std::string &paramName) const
{
  if (deviceTree_.contains("ch") && deviceTree_["ch"].contains(channel) &&
      deviceTree_["ch"][channel].contains("par") &&
      deviceTree_["ch"][channel]["par"].contains(paramName)) {
    return deviceTree_["ch"][channel]["par"][paramName];
  }
  return nlohmann::json();
}

nlohmann::json ParameterValidator::GetRootParameterDefinition(
    const std::string &paramName) const
{
  if (deviceTree_.contains("par") && deviceTree_["par"].contains(paramName)) {
    return deviceTree_["par"][paramName];
  }
  return nlohmann::json();
}

// ============================================================================
// Utility Methods
// ============================================================================

std::string ParameterValidator::FormatValidationMessage(
    const ValidationResult &result) const
{
  std::string symbol = result.isValid ? "✓" : "✗";
  std::string message = symbol + " " + result.paramPath + " = " + result.value;

  if (!result.errorMessage.empty()) {
    message += " (" + result.errorMessage + ")";
  } else if (!result.warningMessage.empty()) {
    message += " (" + result.warningMessage + ")";
  }

  return message;
}

ParameterValidator::ParameterType ParameterValidator::ParseParameterType(
    const nlohmann::json &paramDef) const
{
  if (paramDef.contains("datatype") && paramDef["datatype"].contains("value")) {
    std::string dataType = paramDef["datatype"]["value"];
    std::transform(dataType.begin(), dataType.end(), dataType.begin(),
                   ::tolower);

    if (dataType == "number") return ParameterType::Number;
    if (dataType == "integer") return ParameterType::Integer;
    if (dataType == "string") return ParameterType::String;
    if (dataType == "boolean" || dataType == "bool")
      return ParameterType::Boolean;
    if (dataType == "enum") return ParameterType::Enum;
  }

  return ParameterType::Unknown;
}

void ParameterValidator::PrintValidationReport(
    const ValidationSummary &summary) const
{
  std::cout << "=============================" << std::endl;
  std::cout << "Validation Summary:" << std::endl;
  std::cout << "  Total: " << summary.totalParameters << std::endl;
  std::cout << "  Valid: " << summary.validParameters << std::endl;
  std::cout << "  Invalid: " << summary.invalidParameters << std::endl;
  std::cout << "  Warnings: " << summary.warningParameters << std::endl;
  std::cout << "  Success Rate: " << std::fixed << std::setprecision(1)
            << (summary.GetValidationRate() * 100.0) << "%" << std::endl;
}

std::string ParameterValidator::GenerateValidationReport(
    const ValidationSummary &summary) const
{
  std::ostringstream oss;
  oss << "Validation Report\n";
  oss << "================\n";
  oss << "Total Parameters: " << summary.totalParameters << "\n";
  oss << "Valid: " << summary.validParameters << "\n";
  oss << "Invalid: " << summary.invalidParameters << "\n";
  oss << "Warnings: " << summary.warningParameters << "\n";
  oss << "Success Rate: " << std::fixed << std::setprecision(1)
      << (summary.GetValidationRate() * 100.0) << "%\n\n";

  for (const auto &result : summary.results) {
    oss << FormatValidationMessage(result) << "\n";
  }

  return oss.str();
}

void ParameterValidator::ExportValidationResults(
    const ValidationSummary &summary, const std::string &filename) const
{
  std::ofstream file(filename);
  if (file.is_open()) {
    file << GenerateValidationReport(summary);
    file.close();
  }
}

}  // namespace Digitizer
}  // namespace DELILA