#ifndef DELILA_COMPONENT_DIGITIZER_SOURCE_HPP
#define DELILA_COMPONENT_DIGITIZER_SOURCE_HPP

#include <delila/core/Command.hpp>
#include <delila/core/ComponentConfig.hpp>
#include <delila/core/ComponentState.hpp>
#include <delila/core/ComponentStatus.hpp>
#include <delila/core/IDataComponent.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace DELILA {

// Forward declarations
namespace Net {
class ZMQTransport;
class DataProcessor;
} // namespace Net

/**
 * @brief Data source component that acquires data from digitizer hardware
 *
 * DigitizerSource is a data producer with:
 * - 0 input addresses (data comes from hardware)
 * - 1 output address (sends data downstream)
 *
 * In mock mode, generates synthetic event data for testing.
 *
 * Thread model:
 * - Main thread: State management
 * - Data acquisition thread: Reads from digitizer/mock
 * - Data sending thread: Serializes and sends via ZMQ
 */
class DigitizerSource : public IDataComponent {
public:
  DigitizerSource();
  ~DigitizerSource() override;

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

  // === Public control methods (for direct testing without command channel) ===
  bool Arm();
  bool Start(uint32_t run_number);
  bool Stop(bool graceful);
  void Reset();

  // === Configuration ===
  void SetComponentId(const std::string &id);
  void SetMockMode(bool enable);
  void SetMockEventRate(uint32_t events_per_second);

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
  // State management
  std::atomic<ComponentState> fState{ComponentState::Idle};
  mutable std::mutex fStateMutex;

  // Configuration
  std::string fComponentId;
  std::vector<std::string> fOutputAddresses;
  ComponentConfig fConfig;

  // Mock mode settings
  bool fMockMode = false;
  uint32_t fMockEventRate = 1000; // events per second

  // Run information
  std::atomic<uint32_t> fRunNumber{0};
  std::string fErrorMessage;

  // Metrics
  std::atomic<uint64_t> fEventsProcessed{0};
  std::atomic<uint64_t> fBytesTransferred{0};
  std::atomic<uint64_t> fHeartbeatCounter{0};

  // Worker threads
  std::unique_ptr<std::thread> fAcquisitionThread;
  std::unique_ptr<std::thread> fSendingThread;
  std::atomic<bool> fRunning{false};
  std::atomic<bool> fShutdownRequested{false};

  // Network transport
  std::unique_ptr<Net::ZMQTransport> fTransport;
  std::unique_ptr<Net::DataProcessor> fDataProcessor;

  // Command channel
  std::string fCommandAddress;
  std::unique_ptr<Net::ZMQTransport> fCommandTransport;
  std::unique_ptr<std::thread> fCommandListenerThread;
  std::atomic<bool> fCommandListenerRunning{false};

  // Helper methods
  bool TransitionTo(ComponentState newState);
  void AcquisitionLoop();
  void SendingLoop();
  void GenerateMockEvents();
  void CommandListenerLoop();
  void HandleCommand(const Command &cmd);
};

} // namespace DELILA

#endif // DELILA_COMPONENT_DIGITIZER_SOURCE_HPP
