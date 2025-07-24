#include "DigitizerFactory.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

// Include the concrete digitizer implementations
#include "Digitizer1.hpp"  // Dig1 implementation
#include "Digitizer2.hpp"  // Renamed from original Digitizer class

namespace DELILA
{
namespace Digitizer
{

std::unique_ptr<IDigitizer> DigitizerFactory::CreateDigitizer(
    const ConfigurationManager &config)
{
  std::string url = config.GetParameter("URL");
  if (url.empty()) {
    throw std::runtime_error("URL parameter is required in configuration");
  }

  // Try to get explicit type from configuration first
  std::string typeStr = config.GetParameter("Type");
  FirmwareType type = FirmwareType::UNKNOWN;

  if (!typeStr.empty()) {
    // Convert string to enum
    std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
    if (typeStr == "PSD1")
      type = FirmwareType::PSD1;
    else if (typeStr == "PSD2")
      type = FirmwareType::PSD2;
    else if (typeStr == "PHA1")
      type = FirmwareType::PHA1;
    else if (typeStr == "PHA2")
      type = FirmwareType::PHA2;
    else if (typeStr == "QDC1")
      type = FirmwareType::QDC1;
    else if (typeStr == "SCOPE1")
      type = FirmwareType::SCOPE1;
    else if (typeStr == "SCOPE2")
      type = FirmwareType::SCOPE2;
  }

  // If no explicit type, try to detect from URL
  if (type == FirmwareType::UNKNOWN) {
    type = DetectFirmwareType(url);
  }

  // Create appropriate digitizer based on type
  switch (type) {
    case FirmwareType::PSD1:
    case FirmwareType::PHA1:
    case FirmwareType::QDC1:
    case FirmwareType::SCOPE1:
      // Create Digitizer1 instance (skeleton implementation)
      return std::make_unique<Digitizer1>();

    case FirmwareType::PSD2:
    case FirmwareType::PHA2:
    case FirmwareType::SCOPE2:
      // Create Digitizer2 instance (renamed from original Digitizer class)
      return std::make_unique<Digitizer2>();

    case FirmwareType::UNKNOWN:
    default:
      // Fallback: try to auto-detect or default to dig2
      std::cerr << "Warning: Could not determine digitizer type from URL '"
                << url
                << "'. Defaulting to dig2 (PSD2) for backward compatibility."
                << std::endl;
      return std::make_unique<Digitizer2>();
  }
}

FirmwareType DigitizerFactory::DetectFirmwareType(const std::string &url)
{
  return ParseURL(url);
}

FirmwareType DigitizerFactory::DetectFromDeviceTree(
    const nlohmann::json &deviceTree)
{
  try {
    // Extract firmware type and model name from device tree
    std::string fwType;
    std::string modelName;

    if (deviceTree.contains("par") && deviceTree["par"].contains("fwtype") &&
        deviceTree["par"]["fwtype"].contains("value")) {
      fwType = deviceTree["par"]["fwtype"]["value"];
    }

    if (deviceTree.contains("par") && deviceTree["par"].contains("modelname") &&
        deviceTree["par"]["modelname"].contains("value")) {
      modelName = deviceTree["par"]["modelname"]["value"];
    }

    return AnalyzeFirmware(fwType, modelName);
  } catch (const std::exception &e) {
    std::cerr << "Error analyzing device tree: " << e.what() << std::endl;
    return FirmwareType::UNKNOWN;
  }
}

FirmwareType DigitizerFactory::ParseURL(const std::string &url)
{
  // Convert to lowercase for case-insensitive comparison
  std::string lowerUrl = url;
  std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);

  // Check for explicit scheme prefixes
  if (lowerUrl.find("dig1://") == 0) {
    return FirmwareType::PSD1;  // Default dig1 type
  } else if (lowerUrl.find("dig2://") == 0) {
    return FirmwareType::PSD2;  // Default dig2 type
  } else if (lowerUrl.find("usb://") == 0 || lowerUrl.find("eth://") == 0) {
    // Legacy URLs - need device tree analysis for type detection
    return FirmwareType::UNKNOWN;
  }

  // No recognizable scheme, return unknown for further analysis
  return FirmwareType::UNKNOWN;
}

FirmwareType DigitizerFactory::AnalyzeFirmware(const std::string &fwType,
                                                const std::string &modelName)
{
  // Convert to lowercase for comparison
  std::string lowerFwType = fwType;
  std::string lowerModelName = modelName;
  std::transform(lowerFwType.begin(), lowerFwType.end(), lowerFwType.begin(),
                 ::tolower);
  std::transform(lowerModelName.begin(), lowerModelName.end(),
                 lowerModelName.begin(), ::tolower);

  // Analyze firmware type patterns
  if (lowerFwType.find("dpp-psd") != std::string::npos) {
    return FirmwareType::PSD1;
  } else if (lowerFwType.find("dpp_psd") != std::string::npos ||
             lowerFwType.find("dpp-pha-psd") != std::string::npos) {
    return FirmwareType::PSD2;
  } else if (lowerFwType.find("dpp-pha") != std::string::npos &&
             lowerFwType.find("psd") == std::string::npos) {
    // Check if it's PHA1 or PHA2 based on other indicators
    if (lowerFwType.find("_v2") != std::string::npos ||
        lowerModelName.find("27") != std::string::npos) {
      return FirmwareType::PHA2;
    }
    return FirmwareType::PHA1;
  } else if (lowerFwType.find("dpp-qdc") != std::string::npos) {
    return FirmwareType::QDC1;
  } else if (lowerFwType.find("scope") != std::string::npos ||
             lowerFwType.find("oscilloscope") != std::string::npos) {
    // Distinguish SCOPE1 vs SCOPE2 based on model or firmware version
    if (lowerModelName.find("27") != std::string::npos ||
        lowerFwType.find("_v2") != std::string::npos) {
      return FirmwareType::SCOPE2;
    }
    return FirmwareType::SCOPE1;
  }

  // Fallback: analyze model name if firmware type analysis fails
  if (lowerModelName.find("27") != std::string::npos) {
    // 2nd generation models (dig2)
    return FirmwareType::PSD2;  // Default to PSD2 for 2nd gen
  } else if (lowerModelName.find("25") != std::string::npos ||
             lowerModelName.find("73") != std::string::npos) {
    // 1st generation models (dig1)
    return FirmwareType::PSD1;  // Default to PSD1 for 1st gen
  }

  return FirmwareType::UNKNOWN;
}

}  // namespace Digitizer
}  // namespace DELILA