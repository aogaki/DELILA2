#ifndef IDIGITIZER_HPP
#define IDIGITIZER_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ConfigurationManager.hpp"
#include "EventData.hpp"

namespace DELILA
{
namespace Digitizer
{

enum class FirmwareType {
  PSD1,
  PSD2,
  PHA1,
  PHA2,
  QDC1,
  SCOPE1,
  SCOPE2,
  UNKNOWN
};

class IDigitizer
{
 public:
  virtual ~IDigitizer() = default;

  // Main lifecycle methods
  virtual bool Initialize(const ConfigurationManager &config) = 0;
  virtual bool Configure() = 0;
  virtual bool StartAcquisition() = 0;
  virtual bool StopAcquisition() = 0;

  // Control methods
  virtual bool SendSWTrigger() = 0;
  virtual bool CheckStatus() = 0;

  // Data access
  virtual std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
  GetEventData() = 0;

  // Device information
  virtual void PrintDeviceInfo() = 0;
  virtual const nlohmann::json &GetDeviceTreeJSON() const = 0;
  virtual FirmwareType GetType() const = 0;

  // Module information
  virtual uint64_t GetHandle() const = 0;
  virtual uint8_t GetModuleNumber() const = 0;
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // IDIGITIZER_HPP