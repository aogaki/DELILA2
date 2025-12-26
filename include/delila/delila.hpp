#ifndef DELILA_HPP
#define DELILA_HPP

/**
 * @file delila.hpp
 * @brief Main umbrella header for DELILA2 unified library
 *
 * This header provides access to all DELILA2 components:
 * - Digitizer library for hardware interface and data acquisition
 * - Network library for high-performance data transport and serialization
 *
 * Usage:
 *   #include <delila/delila.hpp>
 *
 * This includes all public APIs. For selective inclusion, use individual headers:
 * - For core data structures: #include "delila/core/EventData.hpp"
 * - For digitizer only: #include "delila/digitizer/Digitizer.hpp"
 * - For network only: #include "delila/net/ZMQTransport.hpp"
 */

// ============================================================================
// CORE LIBRARY HEADERS
// ============================================================================

// Core data structures shared across all DELILA2 components
#include "delila/core/DataType.hpp"
#include "delila/core/EventData.hpp"

// ============================================================================
// DIGITIZER LIBRARY HEADERS
// ============================================================================

// Main digitizer interface
#include "delila/digitizer/Digitizer.hpp"

// Digitizer library implementation headers
#include "delila/digitizer/ConfigurationManager.hpp"
#include "delila/digitizer/Digitizer1.hpp"
#include "delila/digitizer/Digitizer2.hpp"
#include "delila/digitizer/DigitizerFactory.hpp"
#include "delila/digitizer/IDigitizer.hpp"

// Data processing and validation
#include "delila/digitizer/DataValidator.hpp"
#include "delila/digitizer/ParameterValidator.hpp"
#include "delila/digitizer/RawData.hpp"

// Decoder interfaces and implementations
#include "delila/digitizer/IDecoder.hpp"
#include "delila/digitizer/PHA1Decoder.hpp"
#include "delila/digitizer/PSD1Decoder.hpp"
#include "delila/digitizer/PSD2Decoder.hpp"

// Hardware-specific constants and structures
#include "delila/digitizer/PHA1Constants.hpp"
#include "delila/digitizer/PHA1Structures.hpp"
#include "delila/digitizer/PSD1Constants.hpp"
#include "delila/digitizer/PSD1Structures.hpp"
#include "delila/digitizer/PSD2Constants.hpp"
#include "delila/digitizer/PSD2Structures.hpp"

// Utility classes
#include "delila/digitizer/DecoderLogger.hpp"
#include "delila/digitizer/MemoryReader.hpp"

// ============================================================================
// NETWORK LIBRARY HEADERS
// ============================================================================

// Core network transport and serialization
#include "delila/net/Serializer.hpp"
#include "delila/net/ZMQTransport.hpp"

// ============================================================================
// LIBRARY UTILITIES AND VERSION INFO
// ============================================================================

/**
 * @namespace DELILA
 * @brief Main namespace for all DELILA2 components
 */
namespace DELILA
{

/**
 * @brief Get library version information
 * @return Version string in format "major.minor.patch"
 */
const char *getVersion();

/**
 * @brief Initialize DELILA2 library
 * @param config_path Optional path to configuration file
 * @return true if initialization successful
 */
bool initialize(const char *config_path = nullptr);

/**
 * @brief Shutdown DELILA2 library and cleanup resources
 */
void shutdown();

}  // namespace DELILA

#endif  // DELILA_HPP