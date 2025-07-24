#ifndef MEMORYREADER_HPP
#define MEMORYREADER_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Safe memory reading utility for binary data parsing
 *
 * Provides bounds-checked memory access with clear error handling
 * for reading 32-bit words from byte arrays.
 */
class MemoryReader
{
 public:
  /**
   * @brief Construct a MemoryReader with data bounds
   * @param dataStart Iterator to start of data
   * @param totalSizeWords Total size in 32-bit words
   */
  MemoryReader(const std::vector<uint8_t>::iterator &dataStart,
               size_t totalSizeWords);

  /**
   * @brief Read a 32-bit word at the specified index
   * @param wordIndex Index in 32-bit words from start
   * @return The 32-bit word value
   * @throws std::out_of_range if index is beyond bounds
   */
  uint32_t ReadWord32(size_t wordIndex) const;

  /**
   * @brief Safely read a 32-bit word with bounds checking
   * @param wordIndex Index in 32-bit words from start
   * @param value Reference to store the read value
   * @return true if successful, false if out of bounds
   */
  bool ReadWordSafe(size_t wordIndex, uint32_t &value) const;

  /**
   * @brief Check if a word index is within bounds
   * @param wordIndex Index to check
   * @return true if index is valid
   */
  bool IsValidIndex(size_t wordIndex) const;

  /**
   * @brief Get the total size in words
   * @return Total number of 32-bit words
   */
  size_t GetTotalSizeWords() const { return fTotalSizeWords; }

  /**
   * @brief Get remaining words from current position
   * @param currentIndex Current position in words
   * @return Number of words remaining
   */
  size_t GetRemainingWords(size_t currentIndex) const;

  /**
   * @brief Advance index safely with bounds checking
   * @param wordIndex Current index (will be modified)
   * @param count Number of words to advance
   * @return true if advancement is valid, false if would exceed bounds
   */
  bool AdvanceIndex(size_t &wordIndex, size_t count) const;

 private:
  const std::vector<uint8_t>::iterator fDataStart;
  const size_t fTotalSizeWords;
  static constexpr size_t kWordSize = 4;  // 32-bit word size in bytes
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline MemoryReader::MemoryReader(
    const std::vector<uint8_t>::iterator &dataStart, size_t totalSizeWords)
    : fDataStart(dataStart), fTotalSizeWords(totalSizeWords)
{
}

inline uint32_t MemoryReader::ReadWord32(size_t wordIndex) const
{
  if (wordIndex >= fTotalSizeWords) {
    throw std::out_of_range("Word index " + std::to_string(wordIndex) +
                            " exceeds data size " +
                            std::to_string(fTotalSizeWords));
  }

  uint32_t value = 0;
  std::memcpy(&value, &(*(fDataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  return value;
}

inline bool MemoryReader::ReadWordSafe(size_t wordIndex, uint32_t &value) const
{
  if (wordIndex >= fTotalSizeWords) {
    return false;
  }

  std::memcpy(&value, &(*(fDataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  return true;
}

inline bool MemoryReader::IsValidIndex(size_t wordIndex) const
{
  return wordIndex < fTotalSizeWords;
}

inline size_t MemoryReader::GetRemainingWords(size_t currentIndex) const
{
  return (currentIndex < fTotalSizeWords) ? (fTotalSizeWords - currentIndex)
                                          : 0;
}

inline bool MemoryReader::AdvanceIndex(size_t &wordIndex, size_t count) const
{
  if (wordIndex + count > fTotalSizeWords) {
    return false;
  }
  wordIndex += count;
  return true;
}

}  // namespace Digitizer
}  // namespace DELILA

#endif  // MEMORYREADER_HPP