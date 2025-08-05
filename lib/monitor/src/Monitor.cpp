#include "Monitor.hpp"

namespace DELILA::Monitor
{
Monitor::Monitor() {}

Monitor::~Monitor() {}

void Monitor::Start()
{
  fIsRunning = true;
  for (uint32_t i = 0; i < fNThreads; ++i) {
    fThreads.emplace_back(&Monitor::ProcessEvents, this);
  }
}

void Monitor::Stop()
{
  fIsRunning = false;
  for (auto &thread : fThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void Monitor::EnableWebServer(bool enable)
{
  fWebServerEnabled = enable;
  if (fWebServerEnabled) {
    fHttpServer = std::make_unique<THttpServer>("http://localhost:8080");
  } else {
    fHttpServer.reset();
  }
}

void Monitor::SetNThreads(uint32_t nThreads) { fNThreads = nThreads; }

void Monitor::SetHistsParams(
    std::vector<std::vector<HistsParams>> const &params)
{
  fHistsParams = params;
}

void Monitor::CreateADCHists(
    std::vector<std::vector<uint32_t>> const &adcChannels)
{
  fHistograms.clear();
  // Create histogram based on channel parameters and fHistsParams
  // use indexing to access fHistsParams and adcChannels
  for (auto iMod = 0; iMod < adcChannels.size(); ++iMod) {
    std::vector<std::unique_ptr<TH1D>> histogramsForModule;
    for (auto iCh = 0; iCh < adcChannels[iMod].size(); ++iCh) {
      const auto &params = fHistsParams[iMod][iCh];
      auto hist =
          std::make_unique<TH1D>(params.fName.c_str(), params.fTitle.c_str(),
                                 params.fNbinsX, params.fXmin, params.fXmax);
      hist->SetDirectory(nullptr);
      histogramsForModule.push_back(std::move(hist));
    }
    fHistograms.push_back(std::move(histogramsForModule));
  }

  if (fWebServerEnabled && fHttpServer) {
    auto modCounter = 0;
    for (const auto &moduleHists : fHistograms) {
      auto moduleName = "Module_" + std::to_string(modCounter++);
      for (const auto &hist : moduleHists) {
        fHttpServer->Register(moduleName.c_str(), hist.get());
      }
      modCounter++;
    }
  }
}

void Monitor::LoadEventData(
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData)
{
  std::lock_guard<std::mutex> lock(fMutex);
  fEventDataQueue.push_back(std::move(eventData));
}

void Monitor::ProcessEvents()
{
  while (fIsRunning) {
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData;
    {
      std::lock_guard<std::mutex> lock(fMutex);
      if (!fEventDataQueue.empty()) {
        eventData = std::move(fEventDataQueue.front());
        fEventDataQueue.erase(fEventDataQueue.begin());
      }
    }

    if (eventData) {
      // Process the event data
      for (const auto &event : *eventData) {
        auto mod = event->module;
        auto ch = event->channel;
        if (mod < fHistograms.size() && ch < fHistograms[mod].size()) {
          fHistograms[mod][ch]->Fill(event->energy);
        }
      }
    } else {
      // If no event data, sleep for a short duration to avoid busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

}  // namespace DELILA::Monitor