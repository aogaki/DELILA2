#ifndef RECORDER_HPP
#define RECORDER_HPP

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <numeric>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../../../include/delila/core/EventData.hpp"

namespace DELILA
{
namespace Monitor
{

using DELILA::Digitizer::EventData;

class Recorder
{
 public:
  Recorder();
  ~Recorder();

  void StartRecording(uint64_t nThreads = 1,
                      uint64_t fileSizeThreshold = 1024 * 1024 * 1024,
                      uint64_t timeThreshold = 30 * 60, uint64_t runNumber = 0,
                      const std::string &suffix = "test",
                      const std::string &directory = "./");
  void StopRecording();

  void LoadEventData(
      std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData);

  void SetRunNumber(uint64_t runNumber) { fRunNumber = runNumber; }
  void SetSuffix(const std::string &suffix) { fSuffix = suffix; }

 private:
  uint64_t fRunNumber = 0;
  uint64_t fVersion = 0;
  std::string fSuffix = "test";
  std::string fDirectory = "./";

  uint64_t fFileSizeThreshold = 1024 * 1024 * 1024;  // 1 GB
  uint64_t fTimeThreshold = 30 * 60;                 // 30 minutes

  std::thread fDataProcessingThread;
  bool fDataProcessingThreadRunning = false;
  void DataProcessingThread();

  std::vector<std::thread> fWritingThreads;
  void WritingThread(
      std::string fileName,
      std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData);

  std::queue<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>>
      fEventsVectorQueue;
  std::mutex fEventsVectorQueueMutex;

  bool ParseEventData();

  std::vector<std::unique_ptr<EventData>> fEventsVector;
  std::mutex fEventsVectorMutex;

  uint64_t fNSortThreads = 1;
  void ParallelSort();

  // Calculate the size of stored data
  // return size in bytes and number of events
  std::pair<uint64_t, uint64_t> CalStoredDataSize();
};

}  // namespace Monitor
}  // namespace DELILA

#endif  // RECORDER_HPP