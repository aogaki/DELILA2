#ifndef DELILA_HPP
#define DELILA_HPP

/**
 * @file delila.hpp
 * @brief Main header for DELILA2 unified library
 * 
 * This header provides access to all DELILA2 components:
 * - Digitizer library for hardware interface
 * - Network library for data transport
 */

// Forward declarations to avoid symbol conflicts
// Users should include specific headers as needed:
// - For network: #include "lib/net/include/ZMQTransport.hpp"
// - For digitizer: #include "lib/digitizer/include/Digitizer.hpp"

/**
 * @namespace DELILA
 * @brief Main namespace for all DELILA2 components
 */
namespace DELILA {

/**
 * @brief Get library version information
 * @return Version string in format "major.minor.patch"
 */
const char* getVersion();

/**
 * @brief Initialize DELILA2 library
 * @param config_path Optional path to configuration file
 * @return true if initialization successful
 */
bool initialize(const char* config_path = nullptr);

/**
 * @brief Shutdown DELILA2 library and cleanup resources
 */
void shutdown();

}  // namespace DELILA

#endif  // DELILA_HPP