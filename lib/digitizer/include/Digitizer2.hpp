#ifndef DIGITIZER2_HPP
#define DIGITIZER2_HPP

#include <deque>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "ConfigurationManager.hpp"
#include "PSD2Decoder.hpp"
#include "EventData.hpp"
#include "IDigitizer.hpp"
#include "ParameterValidator.hpp"
#include "RawData.hpp"

namespace DELILA
{
namespace Digitizer
{

class Digitizer2 : public IDigitizer
{
 public:
  // Constructor/Destructor
  Digitizer2();
  ~Digitizer2() override;

  // Main lifecycle methods
  bool Initialize(const ConfigurationManager &config) override;
  bool Configure() override;
  bool StartAcquisition() override;
  bool StopAcquisition() override;

  // Data access
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData()
      override;

  // Device information
  const nlohmann::json &GetDeviceTreeJSON() const override
  {
    return fDeviceTree;
  }
  void PrintDeviceInfo() override;
  FirmwareType GetType() const override { return fFirmwareType; }

  // Control methods
  bool SendSWTrigger() override;
  bool CheckStatus() override;

  // Getters
  uint64_t GetHandle() const override { return fHandle; }
  uint8_t GetModuleNumber() const override { return fModuleNumber; }

 private:
  // === Hardware Interface ===
  uint64_t fHandle = 0;
  uint64_t fReadDataHandle = 0;
  uint64_t fRecordLength = 0;
  size_t fMaxRawDataSize = 0;

  // === Configuration ===
  std::string fURL;
  bool fDebugFlag = false;
  uint32_t fNThreads = 1;
  uint8_t fModuleNumber = 0;
  std::vector<std::array<std::string, 2>> fConfig;

  // === Device Information ===
  nlohmann::json fDeviceTree;
  FirmwareType fFirmwareType = FirmwareType::UNKNOWN;

  // === Data Processing ===
  std::unique_ptr<PSD2Decoder> fPSD2Decoder;
  std::unique_ptr<ParameterValidator> fParameterValidator;
  bool fDataTakingFlag = false;
  std::vector<std::thread> fReadDataThreads;
  std::mutex fReadDataMutex;

  // === Event Data Processing ===
  // Note: Event data processing is now handled by Dig2Decoder

  // === Hardware Communication ===
  bool Open(const std::string &url);
  bool Close();
  bool CheckError(int err);
  bool SendCommand(const std::string &path);
  bool GetParameter(const std::string &path, std::string &value);
  bool SetParameter(const std::string &path, const std::string &value);

  // === Device Tree Management ===
  std::string GetDeviceTree();
  void DetermineFirmwareType();

  // === Configuration Validation ===
  bool ValidateParameters();

  // === Configuration Helpers ===
  bool ResetDigitizer();
  bool ApplyConfiguration();
  bool ConfigureRecordLength();
  bool ConfigureMaxRawDataSize();
  bool InitializeDataConverter();
  bool ConfigureSampleRate();

  // === Data Acquisition ===
  bool EndpointConfigure();
  nlohmann::json GetReadDataFormatRAW();
  void ReadDataThread();
  int ReadDataWithLock(std::unique_ptr<RawData_t> &rawData, int timeOut);

  // === Note: All data is automatically converted to EventData ===
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIGITIZER2_HPP