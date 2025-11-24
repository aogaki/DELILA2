/**
 * @file IDigitizer.hpp
 * @brief Abstract digitizer interface for DELILA2 data acquisition system
 * @author DELILA2 Development Team
 * @date 2024
 */

#ifndef IDIGITIZER_HPP
#define IDIGITIZER_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ConfigurationManager.hpp"
#include "../../../include/delila/core/EventData.hpp"

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Supported digitizer firmware types
 * 
 * This enum defines the various firmware types supported by the DELILA2 system.
 * Each firmware type corresponds to different digitizer capabilities and data formats.
 */
enum class FirmwareType {
  PSD1,    ///< Pulse Shape Discrimination v1
  PSD2,    ///< Pulse Shape Discrimination v2
  PHA1,    ///< Pulse Height Analysis v1
  PHA2,    ///< Pulse Height Analysis v2
  AMAX,    ///< AMax (DELILA custom firmware)
  QDC1,    ///< Charge-to-Digital Converter v1
  SCOPE1,  ///< Oscilloscope mode v1
  SCOPE2,  ///< Oscilloscope mode v2
  UNKNOWN  ///< Unknown or unsupported firmware
};;

/**
 * @brief Abstract interface for all digitizer implementations
 * 
 * This interface defines the contract that all digitizer implementations must follow.
 * It provides methods for device lifecycle management, data acquisition, and status monitoring.
 * 
 * @par Usage Example:
 * @code{.cpp}
 * auto digitizer = DigitizerFactory::Create(FirmwareType::PSD1);
 * ConfigurationManager config("digitizer.json");
 * 
 * if (digitizer->Initialize(config) && digitizer->Configure()) {
 *     digitizer->StartAcquisition();
 *     
 *     auto events = digitizer->GetEventData();
 *     // Process events...
 *     
 *     digitizer->StopAcquisition();
 * }
 * @endcode
 * 
 * @par Thread Safety:
 * Implementations are expected to be thread-safe for concurrent access to GetEventData()
 * and status checking methods, but configuration methods should be called from a single thread.
 * 
 * @see DigitizerFactory
 * @see ConfigurationManager
 * @see EventData
 */
class IDigitizer
{
 public:
  /**
   * @brief Virtual destructor for proper cleanup of derived classes
   */
  virtual ~IDigitizer() = default;

  /**
   * @name Lifecycle Management
   * @{
   */
  
  /**
   * @brief Initialize the digitizer with given configuration
   * 
   * This method sets up the digitizer hardware and prepares it for data acquisition.
   * Must be called before any other operations.
   * 
   * @param config Configuration manager containing digitizer settings
   * @return true if initialization successful, false otherwise
   * 
   * @pre Digitizer must be in uninitialized state
   * @post If successful, digitizer is ready for Configure() call
   * 
   * @see ConfigurationManager
   */
  virtual bool Initialize(const ConfigurationManager &config) = 0;
  
  /**
   * @brief Configure the digitizer with loaded parameters
   * 
   * Applies the configuration loaded during Initialize() to the hardware.
   * This includes setting up channels, triggers, and acquisition parameters.
   * 
   * @return true if configuration successful, false otherwise
   * 
   * @pre Initialize() must have been called successfully
   * @post If successful, digitizer is ready for data acquisition
   */
  virtual bool Configure() = 0;
  
  /**
   * @brief Start data acquisition
   * 
   * Begins the data acquisition process. After calling this method,
   * GetEventData() can be used to retrieve acquired events.
   * 
   * @return true if acquisition started successfully, false otherwise
   * 
   * @pre Configure() must have been called successfully
   * @post Data acquisition is active, events can be retrieved
   */
  virtual bool StartAcquisition() = 0;
  
  /**
   * @brief Stop data acquisition
   * 
   * Stops the ongoing data acquisition process. Any remaining events
   * in the buffer can still be retrieved with GetEventData().
   * 
   * @return true if acquisition stopped successfully, false otherwise
   * 
   * @post Data acquisition is stopped, no new events will be generated
   */
  virtual bool StopAcquisition() = 0;
  
  /** @} */

  /**
   * @name Control Operations
   * @{
   */
  
  /**
   * @brief Send software trigger to the digitizer
   * 
   * Generates a software trigger event, useful for testing and calibration.
   * 
   * @return true if trigger sent successfully, false otherwise
   * 
   * @pre Digitizer must be configured and acquisition may be running
   */
  virtual bool SendSWTrigger() = 0;
  
  /**
   * @brief Check current status of the digitizer
   * 
   * Queries the hardware status, including temperature, acquisition state,
   * and error conditions.
   * 
   * @return true if status check successful, false if communication error
   * 
   * @note This method should be non-blocking and safe to call frequently
   */
  virtual bool CheckStatus() = 0;
  
  /** @} */

  /**
   * @name Data Access
   * @{
   */
  
  /**
   * @brief Retrieve acquired event data
   * 
   * Returns all available events from the digitizer buffer. This method
   * should be called regularly during acquisition to prevent buffer overflow.
   * 
   * @return Vector of EventData objects, empty if no events available
   * 
   * @par Performance Notes:
   * - This method uses move semantics for efficient data transfer
   * - Events are removed from internal buffer after retrieval
   * - Safe to call from multiple threads (implementation dependent)
   * 
   * @warning Buffer may overflow if this method is not called frequently enough
   * 
   * @see EventData
   */
  virtual std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
  GetEventData() = 0;
  
  /** @} */

  /**
   * @name Device Information
   * @{
   */
  
  /**
   * @brief Print detailed device information to stdout
   * 
   * Outputs comprehensive information about the digitizer including
   * model, serial number, firmware version, and current configuration.
   * 
   * @note Output format is implementation-specific
   */
  virtual void PrintDeviceInfo() = 0;
  
  /**
   * @brief Get device tree configuration as JSON
   * 
   * Returns the complete device tree configuration that describes
   * the digitizer's capabilities, register map, and current settings.
   * 
   * @return Reference to JSON object containing device tree
   * 
   * @note The returned reference remains valid for the lifetime of the digitizer
   */
  virtual const nlohmann::json &GetDeviceTreeJSON() const = 0;
  
  /**
   * @brief Get the firmware type of this digitizer
   * 
   * @return FirmwareType enum value indicating the digitizer type
   * 
   * @see FirmwareType
   */
  virtual FirmwareType GetType() const = 0;
  
  /** @} */

  /**
   * @name Module Information  
   * @{
   */
  
  /**
   * @brief Get the hardware handle for this digitizer
   * 
   * Returns the unique hardware handle used for low-level communication
   * with the digitizer device.
   * 
   * @return 64-bit hardware handle
   * 
   * @note Handle format is implementation and hardware-specific
   */
  virtual uint64_t GetHandle() const = 0;
  
  /**
   * @brief Get the module number/ID
   * 
   * Returns the logical module number assigned to this digitizer,
   * useful for identifying modules in multi-digitizer setups.
   * 
   * @return 8-bit module number (typically 0-255)
   */
  virtual uint8_t GetModuleNumber() const = 0;
  
  /** @} */
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // IDIGITIZER_HPP