#ifndef DELILA_CORE_IDATACOMPONENT_HPP
#define DELILA_CORE_IDATACOMPONENT_HPP

#include "IComponent.hpp"
#include <string>
#include <vector>

namespace DELILA {

/**
 * @brief Interface for data-handling components
 *
 * Extends IComponent with data input/output capabilities.
 * The number of input/output addresses determines the component's role:
 *
 * | Component       | Inputs | Outputs | Description                    |
 * |-----------------|--------|---------|--------------------------------|
 * | DigitizerSource | 0      | 1       | Hardware data acquisition      |
 * | FileWriter      | 1      | 0       | Data storage                   |
 * | OnlineAnalyzer  | 1      | 0       | Real-time analysis             |
 * | TimeSortMerger  | N      | 1       | Multi-source merge and sort    |
 *
 * Addresses use ZeroMQ format: "tcp://hostname:port"
 */
class IDataComponent : public IComponent {
public:
  virtual ~IDataComponent() = default;

  // === Address Configuration ===

  /**
   * @brief Set input addresses for receiving data
   * @param addresses List of ZeroMQ addresses to connect to
   *
   * Empty list means no input (e.g., DigitizerSource).
   * Called before Initialize() or during configuration.
   */
  virtual void SetInputAddresses(const std::vector<std::string> &addresses) = 0;

  /**
   * @brief Set output addresses for sending data
   * @param addresses List of ZeroMQ addresses to bind/connect
   *
   * Empty list means no output (e.g., FileWriter).
   * Called before Initialize() or during configuration.
   */
  virtual void
  SetOutputAddresses(const std::vector<std::string> &addresses) = 0;

  /**
   * @brief Get current input addresses
   * @return List of input addresses
   */
  virtual std::vector<std::string> GetInputAddresses() const = 0;

  /**
   * @brief Get current output addresses
   * @return List of output addresses
   */
  virtual std::vector<std::string> GetOutputAddresses() const = 0;

  // === Command Channel ===

  /**
   * @brief Set command address for receiving operator commands
   * @param address ZeroMQ REP socket address (e.g., "tcp://*:5557")
   *
   * The component binds to this address and listens for commands
   * from the Operator using REQ/REP pattern.
   */
  virtual void SetCommandAddress(const std::string &address) = 0;

  /**
   * @brief Get current command address
   * @return Command address or empty string if not set
   */
  virtual std::string GetCommandAddress() const = 0;

  /**
   * @brief Start the command listener thread
   *
   * Starts a background thread that listens for commands from the Operator.
   * The listener handles commands and sends responses automatically.
   * Call after Initialize().
   */
  virtual void StartCommandListener() = 0;

  /**
   * @brief Stop the command listener thread
   *
   * Stops the background command listener thread.
   * Called automatically during Shutdown().
   */
  virtual void StopCommandListener() = 0;
};

} // namespace DELILA

#endif // DELILA_CORE_IDATACOMPONENT_HPP
