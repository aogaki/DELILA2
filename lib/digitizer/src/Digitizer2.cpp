#include "Digitizer2.hpp"

#include <CAEN_FELib.h>

#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

#include "AMaxConstants.hpp"
#include "AMaxDecoder.hpp"
#include "PSD2Decoder.hpp"

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Constructor/Destructor
// ============================================================================

Digitizer2::Digitizer2() {}

Digitizer2::~Digitizer2()
{
  SendCommand("/cmd/Reset");
  Close();
}

// ============================================================================
// Main Lifecycle Methods
// ============================================================================

bool Digitizer2::Initialize(const ConfigurationManager &config)
{
  // Get URL from configuration
  fURL = config.GetParameter("URL");
  if (fURL.empty()) {
    std::cerr << "URL is not set in configuration" << std::endl;
    return false;
  }

  // Get debug flag if available
  auto debugStr = config.GetParameter("Debug");
  if (!debugStr.empty()) {
    std::transform(debugStr.begin(), debugStr.end(), debugStr.begin(),
                   ::tolower);
    fDebugFlag = (debugStr == "true" || debugStr == "1" || debugStr == "yes");
  }

  // Get number of threads if available
  auto threadsStr = config.GetParameter("Threads");
  if (!threadsStr.empty()) {
    try {
      fNThreads = std::stoi(threadsStr);
      if (fNThreads < 1) fNThreads = 1;
    } catch (...) {
      fNThreads = 1;
    }
  }

  // Get module ID if available
  auto modIdStr = config.GetParameter("ModID");
  if (!modIdStr.empty()) {
    try {
      int modId = std::stoi(modIdStr);
      if (modId >= 0 && modId <= 255) {
        fModuleNumber = static_cast<uint8_t>(modId);
        std::cout << "Module ID set to: " << static_cast<int>(fModuleNumber)
                  << std::endl;
      }
    } catch (...) {
      fModuleNumber = 0;  // Default to 0 if parsing fails
      std::cout << "Invalid ModID format, using default: 0" << std::endl;
    }
  } else {
    std::cout << "No ModID specified in config, using default: 0" << std::endl;
  }

  // Get all digitizer-specific configuration
  fConfig = config.GetDigitizerConfig();

  // Open the digitizer
  if (Open(fURL)) {
    GetDeviceTree();
    // std::cout << fDeviceTree.dump(2) << std::endl;
    return true;
  }
  return false;
}

bool Digitizer2::Configure()
{
  // Reset the digitizer to a known state
  if (!ResetDigitizer()) {
    return false;
  }

  // Validate parameters before applying them
  if (!ValidateParameters()) {
    std::cerr << "Parameter validation failed. Try configuration." << std::endl;
  }

  // Apply all configuration parameters
  if (!ApplyConfiguration()) {
    return false;
  }

  // Configure record length (skip for AMax - uses registers instead)
  if (fFirmwareType != FirmwareType::AMAX) {
    if (!ConfigureRecordLength()) {
      return false;
    }
  } else {
    // For AMax, set a default record length since it's managed via registers
    fRecordLength = 1000;  // Default value
    std::cout << "AMax firmware: skipping standard record length configuration"
              << std::endl;
  }

  // Configure endpoints for data readout
  if (!EndpointConfigure()) {
    return false;
  }

  // Configure max raw data size
  if (!ConfigureMaxRawDataSize()) {
    return false;
  }

  // Initialize data converter
  if (!InitializeDataConverter()) {
    return false;
  }

  // Configure sample rate
  if (!ConfigureSampleRate()) {
    return false;
  }

  return true;
}

// ============================================================================
// Configuration Helper Methods
// ============================================================================

bool Digitizer2::ResetDigitizer() { return SendCommand("/cmd/Reset"); }

bool Digitizer2::ApplyConfiguration()
{
  // Route to AMax-specific configuration for AMAX firmware
  if (fFirmwareType == FirmwareType::AMAX) {
    return ConfigureAMax();
  }

  // Standard configuration for other firmware types
  bool status = true;
  for (const auto &config : fConfig) {
    // Only use parameters that start with '/' (CAEN digitizer paths)
    if (!config[0].empty() && config[0][0] == '/') {
      status &= SetParameter(config[0], config[1]);
    }
  }
  return status;
}

bool Digitizer2::ConfigureRecordLength()
{
  std::string buf;
  if (!GetParameter("/ch/0/par/ChRecordLengthT", buf)) {
    std::cerr << "Failed to get record length parameter" << std::endl;
    return false;
  }

  auto rl = std::stoi(buf);
  if (rl < 0) {
    std::cerr << "Invalid record length: " << rl << std::endl;
    return false;
  }

  fRecordLength = rl;
  std::cout << "Record length: " << fRecordLength << std::endl;
  return true;
}

bool Digitizer2::ConfigureMaxRawDataSize()
{
  std::string buf;
  if (!GetParameter("/par/MaxRawDataSize", buf)) {
    std::cerr << "Failed to get max raw data size" << std::endl;
    return false;
  }

  fMaxRawDataSize = std::stoi(buf);
  std::cout << "Max raw data size: " << fMaxRawDataSize << std::endl;
  return true;
}

bool Digitizer2::InitializeDataConverter()
{
  // Create appropriate decoder based on firmware type
  if (!fDecoder) {
    if (fFirmwareType == FirmwareType::PSD2) {
      fDecoder = std::make_unique<PSD2Decoder>(fNThreads);
      std::cout << "Created PSD2Decoder" << std::endl;
    } else if (fFirmwareType == FirmwareType::AMAX) {
      fDecoder = std::make_unique<AMaxDecoder>(fNThreads);
      std::cout << "Created AMaxDecoder (stub mode)" << std::endl;
    } else if (fFirmwareType == FirmwareType::PHA2) {
      std::cerr << "Error: PHA2Decoder not yet implemented" << std::endl;
      std::cerr << "Please implement PHA2Decoder class for PHA firmware support"
                << std::endl;
      return false;
    } else if (fFirmwareType == FirmwareType::SCOPE2) {
      std::cerr << "Error: SCOPE2Decoder not yet implemented" << std::endl;
      std::cerr
          << "Please implement SCOPE2Decoder class for SCOPE firmware support"
          << std::endl;
      return false;
    } else {
      std::cerr << "Error: Unsupported firmware type for Digitizer2: "
                << static_cast<int>(fFirmwareType) << std::endl;
      return false;
    }
  }

  fDecoder->SetDumpFlag(fDebugFlag);
  fDecoder->SetModuleNumber(fModuleNumber);
  return true;
}

bool Digitizer2::ConfigureSampleRate()
{
  std::string buf;
  if (!GetParameter("/par/ADC_SamplRate", buf) || buf.empty()) {
    std::cerr << "Failed to get ADC sample rate" << std::endl;
    return false;
  }

  auto adcSamplRateMHz = std::stoi(buf);  // Sample rate in MHz
  if (adcSamplRateMHz <= 0) {
    std::cerr << "Invalid ADC sample rate: " << adcSamplRateMHz << " MHz"
              << std::endl;
    return false;
  }

  // Calculate time per sample in nanoseconds: (1000 ns) / (rate in MHz)
  uint32_t timeStepNs = 1000 / adcSamplRateMHz;
  fDecoder->SetTimeStep(timeStepNs);

  std::cout << "ADC Sample Rate: " << adcSamplRateMHz << " MHz" << std::endl;
  std::cout << "Time step: " << timeStepNs << " ns per sample" << std::endl;

  return true;
}

bool Digitizer2::StartAcquisition()
{
  std::cout << "Start acquisition" << std::endl;

  // Arm the acquisition

  // Initialize EventData storage if not already created

  // Start data acquisition threads
  fDataTakingFlag = true;
  for (uint32_t i = 0; i < fNThreads; i++) {
    fReadDataThreads.emplace_back(&Digitizer2::ReadDataThread, this);
  }

  // Start single EventData conversion thread

  SendCommand("/cmd/ArmAcquisition");

  auto status = true;
  // Only send software start command if StartSource is set to SWcmd
  std::string startSource;
  if (GetParameter("/par/StartSource", startSource) && startSource == "SWcmd") {
    std::cout << "StartSource is SWcmd - waiting before sending software start "
                 "command"
              << std::endl;
    // Short sleep to allow other digitizers to prepare
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "Sending software start command" << std::endl;
    status &= SendCommand("/cmd/SwStartAcquisition");
  } else {
    std::cout << "StartSource is not SWcmd (" << startSource
              << ") - skipping software start command" << std::endl;
  }

  return status;
}

bool Digitizer2::StopAcquisition()
{
  std::cout << "Stop acquisition" << std::endl;

  auto status = SendCommand("/cmd/SwStopAcquisition");
  status &= SendCommand("/cmd/DisarmAcquisition");

  while (true) {
    if (CAEN_FELib_HasData(fReadDataHandle, 100) == CAEN_FELib_Timeout) {
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  fDataTakingFlag = false;

  // Stop data acquisition threads
  for (auto &thread : fReadDataThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  // Stop EventData conversion thread
  fDataTakingFlag = false;  // This will stop conversion thread too

  // Decoder will stop automatically when threads join

  return status;
}

// ============================================================================
// Control Methods
// ============================================================================

bool Digitizer2::SendSWTrigger()
{
  auto err = CAEN_FELib_SendCommand(fHandle, "/cmd/SendSwTrigger");
  CheckError(err);
  return err == CAEN_FELib_Success;
}

bool Digitizer2::CheckStatus()
{
  // Placeholder implementation - can be expanded based on requirements
  return fHandle != 0 && fDataTakingFlag;
}

// ============================================================================
// Data Access
// ============================================================================

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
Digitizer2::GetEventData()
{
  // Get EventData directly from Decoder
  return fDecoder ? fDecoder->GetEventData()
                  : std::make_unique<std::vector<std::unique_ptr<EventData>>>();
}

// ============================================================================
// Hardware Communication
// ============================================================================

bool Digitizer2::EndpointConfigure()
{
  // Configure endpoint
  uint64_t epHandle;
  uint64_t epFolderHandle;
  bool status = true;
  auto err = CAEN_FELib_GetHandle(fHandle, "/endpoint/RAW", &epHandle);
  status &= CheckError(err);
  err = CAEN_FELib_GetParentHandle(epHandle, nullptr, &epFolderHandle);
  status &= CheckError(err);
  err = CAEN_FELib_SetValue(epFolderHandle, "/par/activeendpoint", "RAW");
  status &= CheckError(err);

  // Set data format
  nlohmann::json readDataJSON = GetReadDataFormatRAW();
  std::string readData = readDataJSON.dump();
  err = CAEN_FELib_GetHandle(fHandle, "/endpoint/RAW", &fReadDataHandle);
  status &= CheckError(err);
  err = CAEN_FELib_SetReadDataFormat(fReadDataHandle, readData.c_str());
  status &= CheckError(err);

  return status;
}

int Digitizer2::ReadDataWithLock(std::unique_ptr<RawData_t> &rawData,
                                 int timeOut)
{
  int retCode = CAEN_FELib_Timeout;

  if (fReadDataMutex.try_lock()) {
    if (CAEN_FELib_HasData(fReadDataHandle, timeOut) == CAEN_FELib_Success) {
      retCode =
          CAEN_FELib_ReadData(fReadDataHandle, timeOut, rawData->data.data(),
                              &(rawData->size), &(rawData->nEvents));
    }
    fReadDataMutex.unlock();
  }
  return retCode;
}

void Digitizer2::ReadDataThread()
{
  auto rawData = std::make_unique<RawData_t>(fMaxRawDataSize);
  while (fDataTakingFlag) {
    constexpr auto timeOut = 10;
    auto err = ReadDataWithLock(rawData, timeOut);

    if (err == CAEN_FELib_Success) {
      // Add data through Decoder converter ONLY
      // Data will be converted directly by Decoder
      if (fDecoder) {
        fDecoder->AddData(std::move(rawData));
      }
      rawData = std::make_unique<RawData_t>(fMaxRawDataSize);
    } else if (err == CAEN_FELib_Timeout) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

// ============================================================================
// Data Format Configuration
// ============================================================================

nlohmann::json Digitizer2::GetReadDataFormatRAW()
{
  nlohmann::json readDataJSON;
  nlohmann::json dataJSON;
  dataJSON["name"] = "DATA";
  dataJSON["type"] = "U8";
  dataJSON["dim"] = 1;
  readDataJSON.push_back(dataJSON);
  nlohmann::json sizeJSON;
  sizeJSON["name"] = "SIZE";
  sizeJSON["type"] = "SIZE_T";
  sizeJSON["dim"] = 0;
  readDataJSON.push_back(sizeJSON);
  nlohmann::json nEventsJSON;
  nEventsJSON["name"] = "N_EVENTS";
  nEventsJSON["type"] = "U32";
  nEventsJSON["dim"] = 0;
  readDataJSON.push_back(nEventsJSON);

  return readDataJSON;
}

// ============================================================================
// Error Handling
// ============================================================================

bool Digitizer2::CheckError(int err)
{
  auto errCode = static_cast<CAEN_FELib_ErrorCode>(err);
  if (errCode != CAEN_FELib_Success) {
    std::cout << "\x1b[31m";

    auto errName = std::string(32, '\0');
    CAEN_FELib_GetErrorName(errCode, errName.data());
    std::cerr << "Error code: " << errName << std::endl;

    auto errDesc = std::string(256, '\0');
    CAEN_FELib_GetErrorDescription(errCode, errDesc.data());
    std::cerr << "Error description: " << errDesc << std::endl;

    auto details = std::string(1024, '\0');
    CAEN_FELib_GetLastError(details.data());
    std::cerr << "Details: " << details << std::endl;

    std::cout << "\x1b[0m" << std::endl;
  }

  return errCode == CAEN_FELib_Success;
}

// ============================================================================
// Hardware Parameter Interface
// ============================================================================

bool Digitizer2::SendCommand(const std::string &path)
{
  auto err = CAEN_FELib_SendCommand(fHandle, path.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool Digitizer2::GetParameter(const std::string &path, std::string &value)
{
  char buf[256];
  auto err = CAEN_FELib_GetValue(fHandle, path.c_str(), buf);
  CheckError(err);
  value = std::string(buf);

  return err == CAEN_FELib_Success;
}

bool Digitizer2::SetParameter(const std::string &path, const std::string &value)
{
  auto err = CAEN_FELib_SetValue(fHandle, path.c_str(), value.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool Digitizer2::Open(const std::string &url)
{
  std::cout << "Open URL: " << url << std::endl;

  // Try CAEN_FELib_Open up to 3 times
  constexpr int maxRetries = 3;
  int err = static_cast<int>(CAEN_FELib_InternalError);

  for (int attempt = 1; attempt <= maxRetries; ++attempt) {
    std::cout << "Attempt " << attempt << " of " << maxRetries << std::endl;

    err = CAEN_FELib_Open(url.c_str(), &fHandle);

    if (err == CAEN_FELib_Success) {
      std::cout << "Successfully opened digitizer on attempt " << attempt
                << std::endl;
      return true;
    }

    // Log error for this attempt
    std::cout << "Attempt " << attempt << " failed: ";
    CheckError(err);

    // Wait before retry (except on last attempt)
    if (attempt < maxRetries) {
      std::cout << "Waiting 1 second before retry..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::cout << "All " << maxRetries << " attempts failed" << std::endl;
  return false;
}

bool Digitizer2::Close()
{
  std::cout << "Close digitizer" << std::endl;
  auto err = CAEN_FELib_Close(fHandle);
  CheckError(err);
  return err == CAEN_FELib_Success;
}

// ============================================================================
// Device Tree Management
// ============================================================================

std::string Digitizer2::GetDeviceTree()
{
  if (fHandle == 0) {
    std::cerr << "Digitizer not initialized" << std::endl;
    return "";
  }

  auto jsonSize = CAEN_FELib_GetDeviceTree(fHandle, nullptr, 0) + 1;
  char *json = new char[jsonSize];
  CAEN_FELib_GetDeviceTree(fHandle, json, jsonSize);

  std::string jsonStr = json;
  delete[] json;

  // Parse and store the JSON
  try {
    fDeviceTree = nlohmann::json::parse(jsonStr);
    // Determine digitizer type after parsing device tree
    DetermineFirmwareType();
    // Create parameter validator with device tree
    fParameterValidator = std::make_unique<ParameterValidator>(fDeviceTree);
  } catch (const nlohmann::json::parse_error &e) {
    std::cerr << "Failed to parse device tree JSON: " << e.what() << std::endl;
    fDeviceTree = nlohmann::json();  // Reset to empty JSON
  }

  return jsonStr;
}

void Digitizer2::DetermineFirmwareType()
{
  fFirmwareType = FirmwareType::UNKNOWN;  // Default

  if (!fDeviceTree.contains("par")) {
    return;
  }

  std::string modelName = "";
  std::string fwType = "";

  // Get model name
  if (fDeviceTree["par"].contains("modelname")) {
    modelName = fDeviceTree["par"]["modelname"]["value"];
  }

  // Get firmware type
  if (fDeviceTree["par"].contains("fwtype")) {
    fwType = fDeviceTree["par"]["fwtype"]["value"];
  }

  // Convert to lowercase for easier comparison
  std::transform(modelName.begin(), modelName.end(), modelName.begin(),
                 ::tolower);
  std::transform(fwType.begin(), fwType.end(), fwType.begin(), ::tolower);

  // Debug output to see what we're working with
  std::cout << "DEBUG: Model Name: '" << modelName << "'" << std::endl;
  std::cout << "DEBUG: Firmware Type: '" << fwType << "'" << std::endl;

  // Determine digitizer type based on firmware type and model
  if (fwType.find("psd") != std::string::npos) {
    // Check for DPP_PSD (underscore) = PSD2, DPP-PSD (hyphen) = PSD1
    if (fwType.find("dpp_psd") != std::string::npos) {
      fFirmwareType = FirmwareType::PSD2;
    } else if (fwType.find("dpp-psd") != std::string::npos) {
      fFirmwareType = FirmwareType::PSD1;
    } else {
      // Fall back to model name logic for PSD - extract 4-digit number
      std::string modelNumber = "";
      for (char c : modelName) {
        if (std::isdigit(c)) {
          modelNumber += c;
        }
      }
      if (modelNumber.length() >= 4 && modelNumber[0] == '2') {
        fFirmwareType = FirmwareType::PSD2;
      } else {
        fFirmwareType = FirmwareType::PSD1;
      }
    }
  } else if (fwType.find("pha") != std::string::npos) {
    // Similar logic for PHA
    if (fwType.find("dpp_pha") != std::string::npos) {
      fFirmwareType = FirmwareType::PHA2;
    } else if (fwType.find("dpp-pha") != std::string::npos) {
      fFirmwareType = FirmwareType::PHA1;
    } else {
      // Fall back to model name logic for PHA - extract 4-digit number
      std::string modelNumber = "";
      for (char c : modelName) {
        if (std::isdigit(c)) {
          modelNumber += c;
        }
      }
      if (modelNumber.length() >= 4 && modelNumber[0] == '2') {
        fFirmwareType = FirmwareType::PHA2;
      } else {
        fFirmwareType = FirmwareType::PHA1;
      }
    }
  } else if (fwType.find("qdc") != std::string::npos) {
    fFirmwareType = FirmwareType::QDC1;
  } else if (fwType.find("amax") != std::string::npos ||
             fwType.find("dpp_open") != std::string::npos) {
    // AMax (DELILA custom firmware)
    fFirmwareType = FirmwareType::AMAX;
  } else if (fwType.find("scope") != std::string::npos ||
             fwType.find("oscilloscope") != std::string::npos) {
    // Similar logic for SCOPE
    if (fwType.find("dpp_scope") != std::string::npos ||
        fwType.find("scope_dpp") != std::string::npos) {
      fFirmwareType = FirmwareType::SCOPE2;
    } else if (fwType.find("dpp-scope") != std::string::npos ||
               fwType.find("scope-dpp") != std::string::npos) {
      fFirmwareType = FirmwareType::SCOPE1;
    } else {
      // Fall back to model name logic for SCOPE - extract 4-digit number
      std::string modelNumber = "";
      for (char c : modelName) {
        if (std::isdigit(c)) {
          modelNumber += c;
        }
      }
      if (modelNumber.length() >= 4 && modelNumber[0] == '2') {
        fFirmwareType = FirmwareType::SCOPE2;
      } else {
        fFirmwareType = FirmwareType::SCOPE1;
      }
    }
  }

  // Final fallback: use model name if firmware type didn't match any pattern
  if (fFirmwareType == FirmwareType::UNKNOWN) {
    // Extract just the numeric part from model name like "DT5230" or "VX2730"
    std::string modelNumber = "";
    for (char c : modelName) {
      if (std::isdigit(c)) {
        modelNumber += c;
      }
    }

    if (modelNumber.length() >= 4) {
      char firstDigit = modelNumber[0];
      if (firstDigit == '2') {
        // Need to guess type based on model number ranges
        if (modelNumber.substr(0, 2) == "27") {  // 27xx series typically PSD
          fFirmwareType = FirmwareType::PSD2;
        } else if (modelNumber.substr(0, 3) ==
                   "274") {  // 274x series typically SCOPE
          fFirmwareType = FirmwareType::SCOPE2;
        }
      }
    }
  }
}

// ============================================================================
// Configuration Validation
// ============================================================================

bool Digitizer2::ValidateParameters()
{
  if (!fParameterValidator) {
    std::cerr
        << "Parameter validator not initialized. Device tree may be missing."
        << std::endl;
    return false;
  }

  auto validationSummary = fParameterValidator->ValidateParameters(fConfig);
  return validationSummary.invalidParameters == 0;
}

// ============================================================================
// Device Information Display
// ============================================================================

void Digitizer2::PrintDeviceInfo()
{
  if (fDeviceTree.empty()) {
    std::cerr << "Device tree is empty. Initialize the digitizer first."
              << std::endl;
    return;
  }

  std::cout << "\n=== Device Information ===" << std::endl;

  // Print Model Name from par section
  if (fDeviceTree.contains("par") && fDeviceTree["par"].contains("modelname")) {
    std::string modelName = fDeviceTree["par"]["modelname"]["value"];
    std::cout << "Model Name: " << modelName << std::endl;
  } else {
    std::cout << "Model Name: Not found" << std::endl;
  }

  // Print Serial Number from par section
  if (fDeviceTree.contains("par") && fDeviceTree["par"].contains("serialnum")) {
    std::string serialNum = fDeviceTree["par"]["serialnum"]["value"];
    std::cout << "Serial Number: " << serialNum << std::endl;
  } else {
    std::cout << "Serial Number: Not found" << std::endl;
  }

  // Print Firmware Type from par section
  if (fDeviceTree.contains("par") && fDeviceTree["par"].contains("fwtype")) {
    std::string fwType = fDeviceTree["par"]["fwtype"]["value"];
    std::cout << "Firmware Type: " << fwType << std::endl;
  } else {
    std::cout << "Firmware Type: Not found" << std::endl;
  }

  // Print Digitizer Type
  std::string typeStr;
  switch (fFirmwareType) {
    case FirmwareType::PSD1:
      typeStr = "PSD1";
      break;
    case FirmwareType::PSD2:
      typeStr = "PSD2";
      break;
    case FirmwareType::PHA1:
      typeStr = "PHA1";
      break;
    case FirmwareType::PHA2:
      typeStr = "PHA2";
      break;
    case FirmwareType::AMAX:
      typeStr = "AMAX";
      break;
    case FirmwareType::QDC1:
      typeStr = "QDC1";
      break;
    case FirmwareType::SCOPE1:
      typeStr = "SCOPE1";
      break;
    case FirmwareType::SCOPE2:
      typeStr = "SCOPE2";
      break;
    case FirmwareType::UNKNOWN:
      typeStr = "UNKNOWN";
      break;
  }
  std::cout << "Digitizer Type: " << typeStr << std::endl;

  std::cout << "=========================" << std::endl;
}

// ============================================================================
// AMax-specific Configuration Methods
// ============================================================================

bool Digitizer2::ConfigureAMax()
{
  std::cout << "Configuring AMax (DELILA custom) firmware..." << std::endl;

  bool overallStatus = true;

  // Apply all configuration parameters
  for (const auto &config : fConfig) {
    // Skip empty parameters
    if (config[0].empty()) {
      continue;
    }

    // Only use parameters that start with '/' (CAEN digitizer paths)
    // This filters out non-digitizer parameters like ModID, Threads, URL
    if (config[0][0] != '/') {
      continue;
    }

    // Apply parameter (handles both /reg/ and /par/ paths)
    if (!ApplyAMaxParameter(config[0], config[1])) {
      std::cerr << "Failed to apply: " << config[0] << " = " << config[1]
                << std::endl;
      overallStatus = false;
      // Continue applying other parameters
    }
  }

  if (overallStatus) {
    std::cout << "AMax configuration complete" << std::endl;
  } else {
    std::cerr << "AMax configuration completed with some errors" << std::endl;
  }

  return overallStatus;
}

bool Digitizer2::ApplyAMaxParameter(const std::string &path,
                                    const std::string &value)
{
  try {
    if (IsRegisterPath(path)) {
      // Register access via CAEN_FELib_SetUserRegister
      uint32_t address = ParseRegisterAddress(path);
      uint32_t regValue = std::stoul(value, nullptr, 0);

      if (fDebugFlag) {
        std::cout << "Setting register 0x" << std::hex << address << " = 0x"
                  << regValue << std::dec << " (" << regValue << ")"
                  << std::endl;
      }

      // Call SetUserRegister (assuming it exists in CAEN_FELib)
      // Note: This assumes CAEN_FELib_SetUserRegister is available
      // If not, we would need to use a different approach

      // For now, we'll use the raw register write approach
      // Format: "/reg/0xADDRESS" -> write to user register
      std::string regPath = "/board/userreg/" + std::to_string(address);
      int ret = CAEN_FELib_SetValue(fHandle, regPath.c_str(), value.c_str());

      if (ret != CAEN_FELib_Success) {
        std::cerr << "SetUserRegister failed for address 0x" << std::hex
                  << address << std::dec << " with error code: " << ret
                  << std::endl;
        return false;
      }

      return true;
    } else {
      // Standard parameter access via CAEN_FELib_SetValue
      if (fDebugFlag) {
        std::cout << "Setting parameter " << path << " = " << value
                  << std::endl;
      }

      int ret = CAEN_FELib_SetValue(fHandle, path.c_str(), value.c_str());

      if (ret != CAEN_FELib_Success) {
        std::cerr << "SetValue failed for " << path
                  << " with error code: " << ret << std::endl;
        return false;
      }

      return true;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error applying AMax parameter " << path << ": " << e.what()
              << std::endl;
    return false;
  }
}

bool Digitizer2::IsRegisterPath(const std::string &path) const
{
  // Check if path indicates a register access
  // 1. Path starts with "/reg/"
  // 2. Path starts with "0x" (direct hex address)

  if (path.find("/reg/") == 0) {
    return true;
  }

  if (path.find("0x") == 0 || path.find("0X") == 0) {
    return true;
  }

  return false;
}

uint32_t Digitizer2::ParseRegisterAddress(const std::string &path) const
{
  // Parse register address from various formats:
  // 1. Direct hex: "0x1234"
  // 2. Register path with hex: "/reg/0x1234"
  // 3. Named register: "/reg/hooold"

  // Direct hex address
  if (path.find("0x") == 0 || path.find("0X") == 0) {
    return std::stoul(path, nullptr, 0);
  }

  // Register path with hex address
  if (path.find("/reg/0x") == 0) {
    return std::stoul(path.substr(5), nullptr, 0);
  }

  if (path.find("/reg/0X") == 0) {
    return std::stoul(path.substr(5), nullptr, 0);
  }

  // Named register
  if (path.find("/reg/") == 0) {
    std::string regName = path.substr(5);

    try {
      // Try to find in register map
      return AMaxConstants::GetRegisterAddress(regName);
    } catch (const std::exception &e) {
      // If not found in map, try to parse as decimal
      try {
        return std::stoul(regName, nullptr, 10);
      } catch (...) {
        throw std::runtime_error("Unknown register name: " + regName);
      }
    }
  }

  throw std::runtime_error("Invalid register path format: " + path);
}

}  // namespace Digitizer
}  // namespace DELILA