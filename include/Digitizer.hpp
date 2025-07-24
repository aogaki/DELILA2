#ifndef DIGITIZER_HPP
#define DIGITIZER_HPP

#include <memory>
#include <string>

#include "ConfigurationManager.hpp"
#include "EventData.hpp"
#include "IDigitizer.hpp"

namespace DELILA
{
namespace Digitizer
{

// Backward compatibility wrapper class
// This maintains the original API while using the new factory pattern internally
class Digitizer
{
 public:
  // Constructor/Destructor
  Digitizer();
  ~Digitizer();

  // Main lifecycle methods
  bool Initialize(const ConfigurationManager &config);
  bool Configure();
  bool StartAcquisition();
  bool StopAcquisition();

  // Data access
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData();

  // Device information
  const nlohmann::json &GetDeviceTreeJSON() const;
  void PrintDeviceInfo();

  // Control methods
  bool SendSWTrigger();
  bool CheckStatus();

  // Getters
  uint64_t GetHandle() const;
  uint8_t GetModuleNumber() const;

 private:
  // Internal digitizer implementation (using factory pattern)
  std::unique_ptr<IDigitizer> fDigitizerImpl;
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIGITIZER_HPP