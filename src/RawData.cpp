#include "RawData.hpp"

#include <algorithm>

namespace DELILA
{
namespace Digitizer
{

// Constructor
RawData::RawData(size_t dataSize) : size(0), nEvents(0)
{
  if (dataSize > 0) {
    Resize(dataSize);
  }
}

// Note: Copy and move constructors/operators are now defaulted in header

void RawData::Resize(size_t newSize)
{
  size = newSize;
  data.resize(newSize);
}

void RawData::Clear()
{
  Resize(0);
  nEvents = 0;
}

void RawData::Reserve(size_t capacity) { data.reserve(capacity); }

}  // namespace Digitizer
}  // namespace DELILA