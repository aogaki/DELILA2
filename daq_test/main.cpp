#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

// #include "TH1.h"
// #include "THttpServer.h"

// Include specific DELILA components (avoiding Monitor due to ROOT issues)
#include <delila/delila.hpp>

using DELILA::Digitizer::ConfigurationManager;
using DELILA::Digitizer::Digitizer;
using DELILA::Monitor::Monitor;
using DELILA::Monitor::Recorder;

enum class DAQState { Acquiring, Stopping };

DAQState GetKBInput()
{
  // Set terminal to non-blocking mode
  struct termios old_tio, new_tio;
  tcgetattr(STDIN_FILENO, &old_tio);
  new_tio = old_tio;
  new_tio.c_lflag &= (~ICANON & ~ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

  // Set stdin to non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  char input;
  int result = read(STDIN_FILENO, &input, 1);

  // Restore terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
  fcntl(STDIN_FILENO, F_SETFL, flags);

  if (result > 0) {
    if (input == 'q' || input == 'Q') {
      return DAQState::Stopping;
    } else {
      return DAQState::Acquiring;
    }
  } else {
    // No input available
    return DAQState::Acquiring;
  }
}

int main()
{
  std::cout << "DELILA test program" << std::endl;

  auto digitizer = std::make_unique<Digitizer>();

  auto configManager = ConfigurationManager();
  configManager.LoadFromFile("PSD2.conf");
  digitizer->Initialize(configManager);
  digitizer->Configure();

  auto monitor = std::make_unique<Monitor>();
  monitor->EnableWebServer(true);
  monitor->SetNThreads(4);
  auto histsParams = std::vector<std::vector<DELILA::Monitor::HistsParams>>{
      {{"ADC Histogram", "ADC Channel Histogram", 32000, 0.0, 32000.0}}};
  monitor->SetHistsParams(histsParams);
  auto adcChannels = std::vector<uint32_t>{32};
  monitor->CreateADCHists(adcChannels);
  monitor->Start();

  auto recorder = std::make_unique<Recorder>();
  recorder->StartRecording(4, 1024 * 1024 * 1024, 100000, 1, "test", "./");

  digitizer->StartAcquisition();
  bool isAcquiring = true;

  auto startTime = std::chrono::steady_clock::now();
  double counter = 0;

  while (isAcquiring) {
    auto state = GetKBInput();
    if (state == DAQState::Stopping) {
      isAcquiring = false;
    }
    auto event = digitizer->GetEventData();
    auto nEvents = event->size();
    if (nEvents > 0) {
      counter += nEvents;
      std::cout << "Event received: " << event->size() << std::endl;
      monitor->LoadEventData(std::move(event));
    }
  }

  monitor->Stop();
  digitizer->StopAcquisition();
  recorder->StopRecording();

  auto endTime = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  // Counting rate
  double countingRate = counter / elapsedTime.count();
  std::cout << "Total events: " << counter << std::endl;
  std::cout << "Elapsed time: " << elapsedTime.count() << " seconds"
            << std::endl;
  std::cout << "Counting rate: " << countingRate << " events/second"
            << std::endl;

  return 0;
}