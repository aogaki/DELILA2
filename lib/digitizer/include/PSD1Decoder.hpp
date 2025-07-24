#ifndef PSD1DECODER_HPP
#define PSD1DECODER_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "DataType.hpp"
#include "DataValidator.hpp"
#include "DecoderLogger.hpp"
#include "EventData.hpp"
#include "IDecoder.hpp"
#include "MemoryReader.hpp"
#include "PSD1Constants.hpp"
#include "PSD1Structures.hpp"
#include "RawData.hpp"

namespace DELILA
{
namespace Digitizer
{

class PSD1Decoder : public IDecoder
{
 public:
  // Constructor/Destructor
  explicit PSD1Decoder(uint32_t nThreads = 1);
  ~PSD1Decoder() override;

  // Configuration
  void SetTimeStep(uint32_t timeStep) override
  {
    fTimeStep = timeStep;
    UpdateCachedValues();
  }
  void SetDumpFlag(bool dumpFlag) override
  {
    fDumpFlag = dumpFlag;
    DecoderLogger::SetDebugEnabled(dumpFlag);
  }
  void SetModuleNumber(uint8_t moduleNumber) override { fModuleNumber = moduleNumber; }
  void SetLogLevel(LogLevel level) { DecoderLogger::SetLogLevel(level); }
  void SetEventDataCacheSize(size_t size) { fEventDataCacheSize = size; }

  // === Performance Optimization ===
  void PreAllocateEventData(size_t expectedEvents);
  void UpdateCachedValues();

  // Data Processing
  DataType AddData(std::unique_ptr<RawData_t> rawData) override;
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData() override;

 private:
  // === Configuration ===
  uint32_t fTimeStep = 1;
  bool fDumpFlag = false;
  uint8_t fModuleNumber = 0;

  // === Threading Control ===
  bool fDecodeFlag = false;
  std::vector<std::thread> fDecodeThreads;

  // === Start/Stop State ===
  bool fIsRunning = false;

  // === Raw Data Queue ===
  std::deque<std::unique_ptr<RawData_t>> fRawDataQueue;
  std::mutex fRawDataMutex;

  // === Processed Data Storage ===
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> fEventDataVec;
  std::mutex fEventDataMutex;

  // === Data Processing State ===
  uint64_t fLastCounter = 0;

  // === Performance Optimization Cache ===
  mutable std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
      fEventDataCache;
  size_t fEventDataCacheSize = 1000;  // Default cache size

  // === Pre-computed Values ===
  double fFineTimeMultiplier = 0.0;  // Cached fine time multiplier

  // === Data Type Detection ===
  DataType CheckDataType(std::unique_ptr<RawData_t> &rawData);
  bool CheckStart(std::unique_ptr<RawData_t> &rawData);
  bool CheckStop(std::unique_ptr<RawData_t> &rawData);

  // === Data Processing ===
  void DecodeThread();
  void DecodeData(std::unique_ptr<RawData_t> rawData);

  // === Data Decoding Helpers ===
  void DumpRawData(const RawData_t &rawData) const;
  DecoderResult ValidateDataHeader(uint32_t headerWord, size_t dataSize);
  DecoderResult ProcessEventData(
      const std::vector<uint8_t>::iterator &dataStart, uint32_t totalSize);

  // === Decomposed Processing Methods ===
  DecoderResult ProcessBoardAggregateBlock(
      MemoryReader &reader, size_t &wordIndex,
      std::vector<std::unique_ptr<EventData>> &eventDataVec);
  DecoderResult ProcessChannelPairs(
      MemoryReader &reader, size_t &wordIndex, const BoardHeaderInfo &boardInfo,
      size_t boardEndIndex,
      std::vector<std::unique_ptr<EventData>> &eventDataVec);
  DecoderResult ValidateBlockBounds(size_t currentIndex, size_t endIndex,
                                    size_t totalSize);

  // === PSD1 Format Structures (defined in PSD1Structures.hpp) ===

  // PSD1 decoding methods
  DecoderResult DecodeBoardHeader(MemoryReader &reader, size_t &wordIndex,
                                  BoardHeaderInfo &boardInfo);
  DecoderResult DecodeDualChannelHeader(MemoryReader &reader, size_t &wordIndex,
                                        DualChannelInfo &dualChInfo);

  // === Decomposed Event Decoding Methods ===
  std::unique_ptr<EventData> DecodeEventDirect(
      MemoryReader &reader, size_t &wordIndex,
      const DualChannelInfo &dualChInfo);
  DecoderResult DecodeEventHeader(MemoryReader &reader, size_t &wordIndex,
                                  uint32_t &triggerTimeTag, bool &isOddChannel);
  DecoderResult DecodeEventTimestamp(MemoryReader &reader, size_t &wordIndex,
                                     const DualChannelInfo &dualChInfo,
                                     uint32_t triggerTimeTag,
                                     EventData &eventData);
  DecoderResult DecodeEventDataComponents(MemoryReader &reader,
                                          size_t &wordIndex,
                                          const DualChannelInfo &dualChInfo,
                                          EventData &eventData);

  void DecodeWaveform(MemoryReader &reader, size_t &wordIndex,
                      const DualChannelInfo &dualChInfo, EventData &eventData);
  void DecodeExtrasWord(uint32_t extrasWord, uint8_t extraOption,
                        EventData &eventData, uint16_t &extendedTime,
                        uint16_t &fineTimeStamp);
  void DecodeChargeWord(uint32_t chargeWord, EventData &eventData);
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // PSD1DECODER_HPP