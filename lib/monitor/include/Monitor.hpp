#ifndef MONITOR_HPP
#define MONITOR_HPP

#include <TH1.h>
#include <THttpServer.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../../../include/delila/core/EventData.hpp"

namespace DELILA::Monitor
{
using DELILA::Digitizer::EventData;

class HistsParams
{
 public:
  HistsParams() = default;
  HistsParams(std::string const &name, std::string const &title,
              uint32_t nbinsX, double xmin, double xmax)
      : fName(name), fTitle(title), fNbinsX(nbinsX), fXmin(xmin), fXmax(xmax)
  {
  }

  std::string fName;       ///< Name of the histogram
  std::string fTitle;      ///< Title of the histogram
  uint32_t fNbinsX = 100;  ///< Number of bins in X
  double fXmin = 0.0;      ///< Minimum value in X
  double fXmax = 100.0;    ///< Maximum value in X
};

class Monitor
{
 public:
  Monitor();
  ~Monitor();

  void Start();
  void Stop();

  void EnableWebServer(bool enable = true);

  void SetNThreads(uint32_t nThreads);

  void SetHistsParams(std::vector<std::vector<HistsParams>> const &params);
  void CreateADCHists(std::vector<std::vector<uint32_t>> const &adcChannels);

  void LoadEventData(
      std::unique_ptr<std::vector<std::unique_ptr<EventData>>> eventData);

 private:
  std::vector<std::vector<HistsParams>>
      fHistsParams;  ///< Parameters for histograms

  bool fWebServerEnabled{true};  ///< Flag to enable/disable web server
  std::unique_ptr<THttpServer> fHttpServer;  ///< HTTP server for monitoring

  std::vector<std::vector<std::unique_ptr<TH1D>>> fHistograms;

  bool fIsRunning{false};  ///< Flag to indicate if the monitor is running
  uint32_t fNThreads{4};   ///< Number of threads for processing events
  std::vector<std::thread> fThreads;  ///< Threads for processing events
  std::mutex fMutex;                  ///< Mutex for thread safety
  std::vector<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>>
      fEventDataQueue;  ///< Queue for event data
  void ProcessEvents();
};

}  // namespace DELILA::Monitor

#endif  // MONITOR_HPP