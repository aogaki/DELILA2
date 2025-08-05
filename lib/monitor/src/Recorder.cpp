#include "Recorder.hpp"

#include <TFile.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <future>
#include <iostream>

namespace DELILA
{
namespace Monitor
{

Recorder::Recorder()
{
  // Simple vector-based constructor - no special initialization needed
}

Recorder::~Recorder() {}

void Recorder::StartRecording(uint64_t nThreads, uint64_t fileSizeThreshold,
                              uint64_t timeThreshold, uint64_t runNumber,
                              const std::string &suffix,
                              const std::string &directory)
{
  fFileSizeThreshold = fileSizeThreshold;
  fTimeThreshold = timeThreshold;
  fRunNumber = runNumber;
  fSuffix = suffix;
  fDirectory = directory;
  fVersion = 0;  // Reset version for new recording

  fWritingThreads.clear();
  // Clear the events vector
  {
    std::lock_guard<std::mutex> lock(fEventsVectorMutex);
    fEventsVector.clear();
  }
  fEventsVectorQueue =
      std::queue<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>>();

  fNSortThreads = nThreads;
  fDataProcessingThreadRunning = true;
  fDataProcessingThread = std::thread(&Recorder::DataProcessingThread, this);
}

void Recorder::StopRecording()
{
  fDataProcessingThreadRunning = false;
  if (fDataProcessingThread.joinable()) {
    fDataProcessingThread.join();
  }

  for (auto &thread : fWritingThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  fWritingThreads.clear();
}

void Recorder::LoadEventData(
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData)
{
  if (eventData) {
    std::lock_guard<std::mutex> lock(fEventsVectorQueueMutex);
    fEventsVectorQueue.push(std::move(eventData));
  }
}

bool Recorder::ParseEventData()
{
  std::lock_guard<std::mutex> lock(fEventsVectorQueueMutex);
  if (!fEventsVectorQueue.empty()) {
    std::lock_guard<std::mutex> vectorLock(fEventsVectorMutex);
    auto eventData = std::move(fEventsVectorQueue.front());
    fEventsVectorQueue.pop();

    if (eventData) {
      // Move events from the queue into the vector
      for (auto &event : *eventData) {
        if (event) {
          fEventsVector.push_back(std::move(event));
        }
      }
    }
    return true;
  }
  return false;
}

void Recorder::DataProcessingThread()
{
  uint32_t lastWritingTime = time(nullptr);

  while (fDataProcessingThreadRunning) {
    if (ParseEventData()) {
      auto [dataSize, eventCount] = CalStoredDataSize();
      constexpr double kDataSizeMergingFactor = 1.2;
      if (dataSize >= fFileSizeThreshold * kDataSizeMergingFactor ||
          (time(nullptr) - lastWritingTime) >= fTimeThreshold) {
        ParallelSort();

        uint64_t index = eventCount * fFileSizeThreshold / dataSize;
        if (index == 0) {
          index = 1;  // Ensure at least one event is written
        } else if (index > eventCount) {
          index = eventCount;  // Do not exceed the number of events
        }

        // Extract events from vector
        std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData;
        {
          std::lock_guard<std::mutex> lock(fEventsVectorMutex);
          eventData =
              std::make_unique<std::vector<std::unique_ptr<EventData>>>();
          size_t extractCount =
              std::min(static_cast<size_t>(index), fEventsVector.size());

          // Move first 'extractCount' events to the output vector
          eventData->reserve(extractCount);
          for (size_t i = 0; i < extractCount; ++i) {
            eventData->push_back(std::move(fEventsVector[i]));
          }

          // Remove extracted events from the vector
          fEventsVector.erase(fEventsVector.begin(),
                              fEventsVector.begin() + extractCount);
        }

        std::string fileName =
            TString::Format("run%04llu_v%04llu_%s.root", fRunNumber, fVersion++,
                            fSuffix.c_str())
                .Data();
        fileName = fDirectory + "/" + fileName;
        fWritingThreads.emplace_back(&Recorder::WritingThread, this, fileName,
                                     std::move(eventData));
        lastWritingTime = time(nullptr);
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  // Final cleanup: write remaining events if any
  if (!fEventsVector.empty()) {
    ParallelSort();
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData =
        std::make_unique<std::vector<std::unique_ptr<EventData>>>(
            std::move(fEventsVector));
    fEventsVector.clear();
    std::string fileName =
        TString::Format("run%04llu_v%04llu_%s_final.root", fRunNumber,
                        fVersion++, fSuffix.c_str())
            .Data();
    fileName = fDirectory + "/" + fileName;
    fWritingThreads.emplace_back(&Recorder::WritingThread, this, fileName,
                                 std::move(eventData));
  }
}

std::pair<uint64_t, uint64_t> Recorder::CalStoredDataSize()
{
  std::lock_guard<std::mutex> lock(fEventsVectorMutex);

  uint64_t eventCount = fEventsVector.size();
  uint64_t totalSize = 0;

  // Calculate total size based on events in vector
  for (const auto &event : fEventsVector) {
    if (event) {
      // Base EventData structure size
      totalSize += DELILA::Digitizer::EVENTDATA_SIZE;

      // Add actual waveform data size
      totalSize += event->waveformSize * sizeof(int32_t) *
                   2;  // analogProbe1 + analogProbe2
      totalSize +=
          event->waveformSize * sizeof(uint8_t) * 4;  // digitalProbe1-4
    }
  }

  return {totalSize, eventCount};
}

void Recorder::ParallelSort()
{
  std::lock_guard<std::mutex> lock(fEventsVectorMutex);

  if (fEventsVector.size() <= 1) {
    return;  // Nothing to sort
  }

  // Simple sort by timestamp
  std::sort(fEventsVector.begin(), fEventsVector.end(),
            [](const std::unique_ptr<EventData> &a,
               const std::unique_ptr<EventData> &b) {
              return a->timeStampNs < b->timeStampNs;
            });
}

void Recorder::WritingThread(
    std::string fileName,
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData)
{
  if (!eventData || eventData->empty()) {
    return;  // Nothing to write
  }

  // Create ROOT file
  TFile *file = TFile::Open(fileName.c_str(), "RECREATE");
  if (!file || file->IsZombie()) {
    std::cerr << "Error: Could not create file " << fileName << std::endl;
    if (file) delete file;
    return;
  }

  // Create tree
  TTree *tree = new TTree("EventTree", "Event Data Tree");

  // Create branch data
  uint64_t timestamp;
  std::vector<uint16_t> data;

  tree->Branch("timestamp", &timestamp);
  tree->Branch("data", &data);

  // Write events to tree
  for (const auto &event : *eventData) {
    if (event) {
      timestamp = event->timeStampNs;
      // For now, we'll write the analog probes as data
      data.clear();
      if (event->waveformSize > 0) {
        data.reserve(event->waveformSize);
        for (uint32_t i = 0; i < event->waveformSize; ++i) {
          data.push_back(event->analogProbe1[i]);
        }
      }
      tree->Fill();
    }
  }

  // Write and close file
  tree->Write();
  file->Close();
  delete file;
}

}  // namespace Monitor
}  // namespace DELILA