#ifndef DIGITIZERFACTORY_HPP
#define DIGITIZERFACTORY_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "ConfigurationManager.hpp"
#include "IDigitizer.hpp"

namespace DELILA
{
namespace Digitizer
{

class DigitizerFactory
{
 public:
  static std::unique_ptr<IDigitizer> CreateDigitizer(
      const ConfigurationManager &config);
  static FirmwareType DetectFirmwareType(const std::string &url);
  static FirmwareType DetectFromDeviceTree(const nlohmann::json &deviceTree);

 private:
  static FirmwareType ParseURL(const std::string &url);
  static FirmwareType AnalyzeFirmware(const std::string &fwType,
                                       const std::string &modelName);
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIGITIZERFACTORY_HPP