/**
 * @file MonitorROOT.hpp
 * @brief ROOT-based online monitor component
 *
 * MonitorROOT receives data via ZMQ, generates ROOT histograms,
 * and serves them via THttpServer for web browser visualization.
 */

#ifndef DELILA_COMPONENT_MONITOR_ROOT_HPP
#define DELILA_COMPONENT_MONITOR_ROOT_HPP

#include <delila/core/Command.hpp>
#include <delila/core/ComponentState.hpp>
#include <delila/core/ComponentStatus.hpp>
#include <delila/core/IDataComponent.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Forward declarations for ROOT classes
class TH1F;
class TH1I;
class TH2F;
class TGraph;
class THttpServer;

namespace DELILA {

// Forward declarations
namespace Net {
class ZMQTransport;
class DataProcessor;
}  // namespace Net

/**
 * @brief ROOT-based online monitor for real-time histogram display
 *
 * Receives event data from upstream components (via ZMQ PULL),
 * fills ROOT histograms, and serves them via THttpServer.
 *
 * Features:
 * - Configurable HTTP port for web access
 * - Energy, channel, module, and timing histograms
 * - 2D histograms (Energy vs Channel, Energy vs Module)
 * - Waveform display via TGraph
 * - Automatic histogram reset on new run
 *
 * Thread model:
 * - Main thread: State management
 * - Receive thread: ZMQ data reception
 * - Process thread: Histogram filling
 * - THttpServer runs in its own internal thread
 */
class MonitorROOT : public IDataComponent {
 public:
  MonitorROOT();
  ~MonitorROOT() override;

  // Disable copy
  MonitorROOT(const MonitorROOT&) = delete;
  MonitorROOT& operator=(const MonitorROOT&) = delete;

  // === IComponent interface ===
  bool Initialize(const std::string& config_path) override;
  void Run() override;
  void Shutdown() override;
  ComponentState GetState() const override;
  std::string GetComponentId() const override;
  ComponentStatus GetStatus() const override;

  // === IDataComponent interface ===
  void SetInputAddresses(const std::vector<std::string>& addresses) override;
  void SetOutputAddresses(const std::vector<std::string>& addresses) override;
  std::vector<std::string> GetInputAddresses() const override;
  std::vector<std::string> GetOutputAddresses() const override;

  // === Command channel ===
  void SetCommandAddress(const std::string& address) override;
  std::string GetCommandAddress() const override;
  void StartCommandListener() override;
  void StopCommandListener() override;

  // === Public control methods ===
  bool Arm();
  bool Start(uint32_t run_number);
  bool Stop(bool graceful);
  void Reset();

  // === Configuration ===
  void SetComponentId(const std::string& id);

  /**
   * @brief Set HTTP server port
   * @param port Port number (default: 8080)
   */
  void SetHttpPort(int port);
  int GetHttpPort() const;

  /**
   * @brief Set histogram update interval
   * @param ms Milliseconds between updates (default: 1000)
   */
  void SetUpdateInterval(uint32_t ms);
  uint32_t GetUpdateInterval() const;

  // === Histogram configuration ===

  /**
   * @brief Enable/disable energy histogram
   * @param enable True to enable (default: true)
   */
  void EnableEnergyHistogram(bool enable);
  bool IsEnergyHistogramEnabled() const;

  /**
   * @brief Enable/disable channel histogram
   * @param enable True to enable (default: true)
   */
  void EnableChannelHistogram(bool enable);
  bool IsChannelHistogramEnabled() const;

  /**
   * @brief Enable/disable timing histogram
   * @param enable True to enable (default: false)
   */
  void EnableTimingHistogram(bool enable);
  bool IsTimingHistogramEnabled() const;

  /**
   * @brief Enable/disable 2D histogram (Energy vs Channel)
   * @param enable True to enable (default: false)
   */
  void Enable2DHistogram(bool enable);
  bool Is2DHistogramEnabled() const;

  // === Waveform display configuration ===

  /**
   * @brief Enable/disable waveform display
   * @param enable True to enable (default: false)
   */
  void EnableWaveformDisplay(bool enable);
  bool IsWaveformDisplayEnabled() const;

  /**
   * @brief Set the channel for waveform display
   * @param module Module number
   * @param channel Channel number
   */
  void SetWaveformChannel(uint8_t module, uint8_t channel);
  std::pair<uint8_t, uint8_t> GetWaveformChannel() const;

  // === Testing utilities ===
  void ForceError(const std::string& message);

 protected:
  // === IComponent callbacks ===
  bool OnConfigure(const nlohmann::json& config) override;
  bool OnArm() override;
  bool OnStart(uint32_t run_number) override;
  bool OnStop(bool graceful) override;
  void OnReset() override;

 private:
  // === Helper methods ===
  bool TransitionTo(ComponentState newState);
  void ReceiveLoop();
  void CommandListenerLoop();
  void HandleCommand(const Command& cmd);

  // === Histogram management ===
  void CreateHistograms();
  void ResetHistograms();
  void DeleteHistograms();

  // === State ===
  std::atomic<ComponentState> fState{ComponentState::Idle};
  mutable std::mutex fStateMutex;
  std::string fComponentId;

  // === Addresses ===
  std::vector<std::string> fInputAddresses;

  // === Configuration ===
  int fHttpPort{8080};
  uint32_t fUpdateInterval{1000};

  // === Histogram enables ===
  bool fEnableEnergyHist{true};
  bool fEnableChannelHist{true};
  bool fEnableTimingHist{false};
  bool fEnable2DHist{false};
  bool fEnableWaveform{false};
  uint8_t fWaveformModule{0};
  uint8_t fWaveformChannel{0};

  // === Run state ===
  std::atomic<uint32_t> fRunNumber{0};
  std::string fErrorMessage;
  std::atomic<uint64_t> fEventsProcessed{0};
  std::atomic<uint64_t> fBytesTransferred{0};
  std::atomic<uint64_t> fHeartbeatCounter{0};

  // === Threads ===
  std::unique_ptr<std::thread> fReceiveThread;
  std::atomic<bool> fRunning{false};
  std::atomic<bool> fShutdownRequested{false};

  // === Network components ===
  std::unique_ptr<Net::ZMQTransport> fTransport;
  std::unique_ptr<Net::DataProcessor> fDataProcessor;

  // === ROOT components ===
  std::unique_ptr<THttpServer> fHttpServer;

  // Histograms (owned by ROOT, but we track pointers)
  TH1F* fHEnergy{nullptr};
  TH1I* fHChannel{nullptr};
  TH1I* fHModule{nullptr};
  TH1F* fHTimeDiff{nullptr};
  TH2F* fHEnergyVsChannel{nullptr};
  TH2F* fHEnergyVsModule{nullptr};
  TGraph* fGWaveform{nullptr};

  // Mutex for histogram access
  mutable std::mutex fHistMutex;

  // Previous timestamp for time difference calculation
  double fPreviousTimestamp{0.0};

  // === Command channel ===
  std::string fCommandAddress;
  std::unique_ptr<Net::ZMQTransport> fCommandTransport;
  std::unique_ptr<std::thread> fCommandListenerThread;
  std::atomic<bool> fCommandListenerRunning{false};
};

}  // namespace DELILA

#endif  // DELILA_COMPONENT_MONITOR_ROOT_HPP
