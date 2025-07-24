#ifndef PARAMETERVALIDATOR_HPP
#define PARAMETERVALIDATOR_HPP

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Advanced parameter validation for digitizer configurations
 *
 * This class provides comprehensive validation of digitizer parameters
 * using device tree definitions, with support for custom validation rules,
 * detailed error reporting, and extensible validation types.
 */
class ParameterValidator
{
 public:
  // Validation result structure
  struct ValidationResult {
    bool isValid = false;
    std::string paramPath;
    std::string value;
    std::string errorMessage;
    std::string warningMessage;

    ValidationResult() = default;
    ValidationResult(bool valid, std::string path, std::string val,
                     std::string error = "", std::string warning = "")
        : isValid(valid),
          paramPath(std::move(path)),
          value(std::move(val)),
          errorMessage(std::move(error)),
          warningMessage(std::move(warning))
    {
    }
  };

  // Validation summary
  struct ValidationSummary {
    size_t totalParameters = 0;
    size_t validParameters = 0;
    size_t invalidParameters = 0;
    size_t unknownParameters = 0;
    size_t warningParameters = 0;
    std::vector<ValidationResult> results;

    double GetValidationRate() const
    {
      return totalParameters > 0
                 ? static_cast<double>(validParameters) / totalParameters
                 : 0.0;
    }
  };

  // Parameter data types
  enum class ParameterType {
    Unknown,
    Number,
    Integer,
    String,
    Boolean,
    Enum,
    Array
  };

  // Custom validation function type
  using CustomValidator = std::function<ValidationResult(
      const std::string &path, const std::string &value,
      const nlohmann::json &paramDef)>;

  // Constructors and Destructor
  explicit ParameterValidator(const nlohmann::json &deviceTree);
  ~ParameterValidator() = default;

  // Copy semantics
  ParameterValidator(const ParameterValidator &other) = default;
  ParameterValidator &operator=(const ParameterValidator &other) = default;

  // Move semantics
  ParameterValidator(ParameterValidator &&other) noexcept = default;
  ParameterValidator &operator=(ParameterValidator &&other) noexcept = default;

  // Main validation methods
  ValidationSummary ValidateParameters(
      const std::vector<std::array<std::string, 2>> &config);
  ValidationResult ValidateParameter(const std::string &paramPath,
                                     const std::string &value);
  ValidationResult ValidateSingleParameter(const std::string &paramPath,
                                           const std::string &value);

  // Validation configuration
  void SetAllowUnknownParameters(bool allow)
  {
    allowUnknownParameters_ = allow;
  }
  void SetStrictMode(bool strict) { strictMode_ = strict; }
  void SetVerboseOutput(bool verbose) { verboseOutput_ = verbose; }
  void SetSilentMode(bool silent) { silentMode_ = silent; }

  // Custom validation rules
  void AddCustomValidator(const std::string &paramPattern,
                          CustomValidator validator);
  void RemoveCustomValidator(const std::string &paramPattern);
  void ClearCustomValidators();

  // Parameter information
  ParameterType GetParameterType(const std::string &paramPath) const;
  std::optional<std::string> GetParameterDescription(
      const std::string &paramPath) const;
  std::optional<std::pair<std::string, std::string>> GetParameterRange(
      const std::string &paramPath) const;
  std::vector<std::string> GetAllowedValues(const std::string &paramPath) const;

  // Device tree utilities
  bool IsParameterSupported(const std::string &paramPath) const;
  std::vector<std::string> GetSupportedParameters() const;
  std::vector<std::string> GetChannelParameters(
      const std::string &channel) const;

  // Error and warning management
  void AddIgnorePattern(const std::string &pattern);
  void RemoveIgnorePattern(const std::string &pattern);
  bool IsIgnored(const std::string &paramPath) const;

  // Statistics and reporting
  void PrintValidationReport(const ValidationSummary &summary) const;
  std::string GenerateValidationReport(const ValidationSummary &summary) const;
  void ExportValidationResults(const ValidationSummary &summary,
                               const std::string &filename) const;

 private:
  // Data members
  const nlohmann::json &deviceTree_;
  bool allowUnknownParameters_ = false;
  bool strictMode_ = false;
  bool verboseOutput_ = false;
  bool silentMode_ = false;

  std::map<std::string, CustomValidator> customValidators_;
  std::unordered_set<std::string> ignorePatterns_;

  // Parameter path processing
  std::vector<std::string> ExpandChannelRange(
      const std::string &configPath) const;
  std::string MapConfigToDeviceTree(const std::string &configParam) const;
  bool MatchesPattern(const std::string &path,
                      const std::string &pattern) const;

  // Validation engine
  ValidationResult ValidateParameterValue(const nlohmann::json &paramDef,
                                          const std::string &paramPath,
                                          const std::string &value) const;
  ValidationResult ValidateNumberParameter(const nlohmann::json &paramDef,
                                           const std::string &paramPath,
                                           const std::string &value) const;
  ValidationResult ValidateIntegerParameter(const nlohmann::json &paramDef,
                                            const std::string &paramPath,
                                            const std::string &value) const;
  ValidationResult ValidateStringParameter(const nlohmann::json &paramDef,
                                           const std::string &paramPath,
                                           const std::string &value) const;
  ValidationResult ValidateBooleanParameter(const nlohmann::json &paramDef,
                                            const std::string &paramPath,
                                            const std::string &value) const;
  ValidationResult ValidateEnumParameter(const nlohmann::json &paramDef,
                                         const std::string &paramPath,
                                         const std::string &value) const;

  // Device tree navigation
  nlohmann::json GetParameterDefinition(const std::string &paramPath) const;
  nlohmann::json GetChannelParameterDefinition(
      const std::string &channel, const std::string &paramName) const;
  nlohmann::json GetRootParameterDefinition(const std::string &paramName) const;

  // Utility methods
  std::string FormatValidationMessage(const ValidationResult &result) const;
  ParameterType ParseParameterType(const nlohmann::json &paramDef) const;
  bool IsNumericType(ParameterType type) const;
  std::string TrimWhitespace(const std::string &str) const;
  std::vector<std::string> SplitString(const std::string &str,
                                       char delimiter) const;
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // PARAMETERVALIDATOR_HPP