#pragma once

/**
 * @file delila_net_binary.hpp
 * @brief Main include file for DELILA2 binary serialization module
 * 
 * This header provides the complete binary serialization API for DELILA2 DAQ system.
 * It includes error handling, platform utilities, and the main BinarySerializer class.
 */

#include "utils/Error.hpp"
#include "utils/Platform.hpp"
#include "serialization/BinarySerializer.hpp"

namespace DELILA::Net {

// Version information
constexpr const char* BINARY_VERSION = "1.0.0";
constexpr int BINARY_VERSION_MAJOR = 1;
constexpr int BINARY_VERSION_MINOR = 0;
constexpr int BINARY_VERSION_PATCH = 0;

} // namespace DELILA::Net