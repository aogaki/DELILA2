/**
 * @file TimeSortMerger.hpp
 * @brief Time-sorting merger component for multiple data sources
 *
 * TimeSortMerger receives data from N upstream sources, sorts events
 * by timestamp, and outputs a merged, time-ordered stream.
 */

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "delila/core/Command.hpp"
#include "delila/core/ComponentState.hpp"
#include "delila/core/ComponentStatus.hpp"
#include "delila/core/IDataComponent.hpp"

namespace DELILA {

namespace Net {
class ZMQTransport;
class DataProcessor;
class EOSTracker;
}  // namespace Net

/**
 * @brief Merges multiple data streams with time-based sorting
 *
 * This component:
 * - Receives data from N input sources (PULL sockets)
 * - Buffers events within a configurable time window
 * - Sorts events by timestamp
 * - Outputs merged, time-ordered stream (PUSH socket)
 *
 * State transitions follow IComponent standard:
 *   Idle -> Configured -> Armed -> Running -> Configured
 */
class TimeSortMerger : public IDataComponent {
public:
  TimeSortMerger();
  ~TimeSortMerger() override;

  // Disable copy
  TimeSortMerger(const TimeSortMerger &) = delete;
  TimeSortMerger &operator=(const TimeSortMerger &) = delete;

  // === IComponent interface ===
  bool Initialize(const std::string &config_path) override;
  void Run() override;
  void Shutdown() override;
  ComponentState GetState() const override;
  std::string GetComponentId() const override;
  ComponentStatus GetStatus() const override;

  // === IDataComponent interface ===
  void SetInputAddresses(const std::vector<std::string> &addresses) override;
  void SetOutputAddresses(const std::vector<std::string> &addresses) override;
  std::vector<std::string> GetInputAddresses() const override;
  std::vector<std::string> GetOutputAddresses() const override;

  // === Command channel ===
  void SetCommandAddress(const std::string &address) override;
  std::string GetCommandAddress() const override;
  void StartCommandListener() override;
  void StopCommandListener() override;

  // === Public control methods ===
  bool Arm();
  bool Start(uint32_t run_number);
  bool Stop(bool graceful);
  void Reset();

  // === Configuration ===
  void SetComponentId(const std::string &id);

  /**
   * @brief Set the sort window size in nanoseconds
   * @param window_ns Time window for buffering and sorting
   *
   * Events are buffered until they are older than (newest_timestamp - window_ns).
   * Larger windows provide better sorting but increase latency and memory usage.
   */
  void SetSortWindowNs(uint64_t window_ns);

  /**
   * @brief Get the current sort window size
   * @return Sort window in nanoseconds
   */
  uint64_t GetSortWindowNs() const;

  /**
   * @brief Get the number of configured input sources
   * @return Number of input addresses
   */
  size_t GetInputCount() const;

  // === Testing utilities ===
  void ForceError(const std::string &message);

protected:
  // === IComponent callbacks ===
  bool OnConfigure(const nlohmann::json &config) override;
  bool OnArm() override;
  bool OnStart(uint32_t run_number) override;
  bool OnStop(bool graceful) override;
  void OnReset() override;

private:
  // === Helper methods ===
  bool TransitionTo(ComponentState newState);
  void ReceivingLoop(size_t input_index);
  void MergingLoop();
  void SendingLoop();

  // === State ===
  std::atomic<ComponentState> fState{ComponentState::Idle};
  mutable std::mutex fStateMutex;
  std::string fComponentId;

  // === Addresses ===
  std::vector<std::string> fInputAddresses;
  std::vector<std::string> fOutputAddresses;

  // === Configuration ===
  uint64_t fSortWindowNs{10000000};  // Default: 10ms

  // === Run state ===
  std::atomic<uint32_t> fRunNumber{0};
  std::string fErrorMessage;
  std::atomic<uint64_t> fEventsProcessed{0};
  std::atomic<uint64_t> fBytesTransferred{0};
  std::atomic<uint64_t> fHeartbeatCounter{0};

  // === Threads ===
  std::vector<std::unique_ptr<std::thread>> fReceivingThreads;
  std::unique_ptr<std::thread> fMergingThread;
  std::unique_ptr<std::thread> fSendingThread;
  std::atomic<bool> fRunning{false};
  std::atomic<bool> fShutdownRequested{false};

  // === Network components ===
  std::vector<std::unique_ptr<Net::ZMQTransport>> fInputTransports;
  std::unique_ptr<Net::ZMQTransport> fOutputTransport;
  std::unique_ptr<Net::DataProcessor> fDataProcessor;
  std::unique_ptr<Net::EOSTracker> fEOSTracker;

  // === Command channel ===
  std::string fCommandAddress;
  std::unique_ptr<Net::ZMQTransport> fCommandTransport;
  std::unique_ptr<std::thread> fCommandListenerThread;
  std::atomic<bool> fCommandListenerRunning{false};
  void CommandListenerLoop();
  void HandleCommand(const Command &cmd);
};

}  // namespace DELILA
