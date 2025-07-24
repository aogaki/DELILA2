#ifndef IDECODER_HPP
#define IDECODER_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "DataType.hpp"
#include "EventData.hpp"
#include "RawData.hpp"

namespace DELILA
{
namespace Digitizer
{

class IDecoder
{
 public:
  virtual ~IDecoder() = default;

  // Configuration methods
  virtual void SetTimeStep(uint32_t timeStep) = 0;
  virtual void SetDumpFlag(bool dumpFlag) = 0;
  virtual void SetModuleNumber(uint8_t moduleNumber) = 0;

  // Data processing methods
  virtual DataType AddData(std::unique_ptr<RawData_t> rawData) = 0;
  virtual std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData() = 0;
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // IDECODER_HPP