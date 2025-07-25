#ifndef DELILA_NET_HPP
#define DELILA_NET_HPP

/**
 * @file delila_net.hpp
 * @brief Main header for DELILA Network Library
 * 
 * This header includes all public components of the DELILA networking library.
 * Include individual headers for specific functionality if needed.
 */

// Core components
#include "serialization/BinarySerializer.hpp"
#include "transport/Connection.hpp"
#include "transport/NetworkModule.hpp"
#include "config/NetworkConfig.hpp"
#include "utils/Error.hpp"
#include "utils/SequenceTracker.hpp"

// Protocol buffer messages
#include "proto/control_messages.pb.h"

/**
 * @namespace DELILA::Net
 * @brief Main namespace for DELILA networking library
 */
namespace DELILA::Net {
    // Version information
    constexpr int VERSION_MAJOR = 1;
    constexpr int VERSION_MINOR = 0;
    constexpr int VERSION_PATCH = 0;
    constexpr const char* VERSION_STRING = "1.0.0";
}

#endif // DELILA_NET_HPP