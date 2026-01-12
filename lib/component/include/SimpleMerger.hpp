/**
 * @file SimpleMerger.hpp
 * @brief Simple merger component for multiple data sources
 *
 * SimpleMerger receives data from N upstream sources and forwards
 * them to a single downstream output without sorting.
 * Sorting is left to downstream components if needed.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
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
 * @brief Merges multiple data streams without sorting
 *
 * This component:
 * - Receives data from N input sources (PULL sockets)
 * - Buffers data in a thread-safe queue
 * - Forwards data to downstream (PUSH socket)
 * - No sorting - downstream handles sorting if needed
 *
 * Architecture:
 *   N ReceivingThreads -> Queue -> 1 SendingThread
 *
 * State transitions follow IComponent standard:
 *   Idle -> Configured -> Armed -> Running -> Configured
 */
class SimpleMerger : public IDataComponent {
public:
  SimpleMerger();
  ~SimpleMerger() override;

  // Disable copy
  SimpleMerger(const SimpleMerger &) = delete;
  SimpleMerger &operator=(const SimpleMerger &) = delete;

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
   * @brief Get the number of configured input sources
   * @return Number of input addresses
   */
  size_t GetInputCount() const;

  /**
   * @brief Get current queue size
   * @return Number of items in queue
   */
  size_t GetQueueSize() const;

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
  void SendingLoop();

  // === State ===
  std::atomic<ComponentState> fState{ComponentState::Idle};
  mutable std::mutex fStateMutex;
  std::string fComponentId;

  // === Addresses ===
  std::vector<std::string> fInputAddresses;
  std::vector<std::string> fOutputAddresses;

  // === Run state ===
  std::atomic<uint32_t> fRunNumber{0};
  std::string fErrorMessage;
  std::atomic<uint64_t> fEventsProcessed{0};
  std::atomic<uint64_t> fBytesTransferred{0};
  std::atomic<uint64_t> fHeartbeatCounter{0};

  // === Thread-safe queue for data buffering ===
  std::queue<std::unique_ptr<std::vector<uint8_t>>> fDataQueue;
  mutable std::mutex fQueueMutex;
  std::condition_variable fQueueCondition;
  static constexpr size_t kMaxQueueSize = 10000;  // Prevent unbounded growth

  // === Threads ===
  std::vector<std::unique_ptr<std::thread>> fReceivingThreads;
  std::unique_ptr<std::thread> fSendingThread;
  std::atomic<bool> fRunning{false};
  std::atomic<bool> fShutdownRequested{false};

  // === Network components ===
  std::vector<std::unique_ptr<Net::ZMQTransport>> fInputTransports;
  std::unique_ptr<Net::ZMQTransport> fOutputTransport;
  std::unique_ptr<Net::DataProcessor> fDataProcessor;
  std::unique_ptr<Net::EOSTracker> fEOSTracker;

  // === EOS tracking ===
  std::atomic<size_t> fEOSReceivedCount{0};

  // === Command channel ===
  std::string fCommandAddress;
  std::unique_ptr<Net::ZMQTransport> fCommandTransport;
  std::unique_ptr<std::thread> fCommandListenerThread;
  std::atomic<bool> fCommandListenerRunning{false};
  void CommandListenerLoop();
  void HandleCommand(const Command &cmd);
};

}  // namespace DELILA
