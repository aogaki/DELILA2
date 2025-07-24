#ifndef RAWDATA_HPP
#define RAWDATA_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Raw data container for digitizer data
 *
 * This class represents raw data buffer from a digitizer, containing
 * binary data along with size and event count information.
 * Uses public-only variables for simplicity and consistency.
 */
class RawData
{
 public:
  // Constructors and Destructor
  explicit RawData(size_t dataSize = 0);
  ~RawData() = default;

  // Copy semantics
  RawData(const RawData &other) = default;
  RawData &operator=(const RawData &other) = default;

  // Move semantics
  RawData(RawData &&other) noexcept = default;
  RawData &operator=(RawData &&other) noexcept = default;

  // Utility methods
  void Resize(size_t newSize);
  void Clear();
  void Reserve(size_t capacity);
  bool IsEmpty() const { return data.empty(); }
  size_t GetCapacity() const { return data.capacity(); }

  // Public data members (direct access)
  std::vector<uint8_t> data;
  size_t size = 0;
  uint32_t nEvents = 0;
};

// Type aliases
using RawData_t = RawData;
using RAWDATA_t = RawData;  // Legacy compatibility

}  // namespace Digitizer
}  // namespace DELILA

#endif  // RAWDATA_HPP