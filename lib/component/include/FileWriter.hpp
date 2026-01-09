#ifndef DELILA_COMPONENT_FILE_WRITER_HPP
#define DELILA_COMPONENT_FILE_WRITER_HPP

#include <delila/core/Command.hpp>
#include <delila/core/ComponentConfig.hpp>
#include <delila/core/ComponentState.hpp>
#include <delila/core/ComponentStatus.hpp>
#include <delila/core/IDataComponent.hpp>

#include <atomic>
#include <fstream>
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
 * @brief Data sink component that writes event data to files
 *
 * FileWriter is a data consumer with:
 * - 1 input address (receives data from upstream)
 * - 0 output addresses (data terminates here)
 *
 * Files are written in binary format with run number in filename.
 * Example: run_000042.dat
 *
 * Thread model:
 * - Main thread: State management
 * - Data receiving thread: Receives and deserializes from ZMQ
 * - File writing thread: Writes data to disk
 */
class FileWriter : public IDataComponent {
public:
  FileWriter();
  ~FileWriter() override;

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
  void SetOutputPath(const std::string &path);
  std::string GetOutputPath() const;
  void SetFilePrefix(const std::string &prefix);
  std::string GetFilePrefix() const;

  // === Testing utilities ===
  void ForceError(const std::string &message);

  // === EOS (End Of Stream) tracking ===
  bool HasReceivedEOS() const;
  void ResetEOSFlag();

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
  std::vector<std::string> fInputAddresses;
  ComponentConfig fConfig;

  // File settings
  std::string fOutputPath;
  std::string fFilePrefix = "run_";

  // Run information
  std::atomic<uint32_t> fRunNumber{0};
  std::string fErrorMessage;

  // Metrics
  std::atomic<uint64_t> fEventsProcessed{0};
  std::atomic<uint64_t> fBytesTransferred{0};
  std::atomic<uint64_t> fHeartbeatCounter{0};

  // Worker threads
  std::unique_ptr<std::thread> fReceivingThread;
  std::unique_ptr<std::thread> fWritingThread;
  std::atomic<bool> fRunning{false};
  std::atomic<bool> fShutdownRequested{false};

  // Network transport
  std::unique_ptr<Net::ZMQTransport> fTransport;
  std::unique_ptr<Net::DataProcessor> fDataProcessor;

  // File output
  std::unique_ptr<std::ofstream> fOutputFile;

  // Command channel
  std::string fCommandAddress;
  std::unique_ptr<Net::ZMQTransport> fCommandTransport;
  std::unique_ptr<std::thread> fCommandListenerThread;
  std::atomic<bool> fCommandListenerRunning{false};

  // EOS tracking
  std::atomic<bool> fReceivedEOS{false};

  // Helper methods
  bool TransitionTo(ComponentState newState);
  void ReceivingLoop();
  void WritingLoop();
  std::string GenerateFilename(uint32_t run_number) const;
  bool OpenOutputFile(uint32_t run_number);
  void CloseOutputFile();
  void CommandListenerLoop();
  void HandleCommand(const Command &cmd);
};

} // namespace DELILA

#endif // DELILA_COMPONENT_FILE_WRITER_HPP
