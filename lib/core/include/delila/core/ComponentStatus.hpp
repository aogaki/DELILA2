#ifndef DELILA_CORE_COMPONENT_STATUS_HPP
#define DELILA_CORE_COMPONENT_STATUS_HPP

#include "ComponentState.hpp"
#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief Performance metrics for a component
 */
struct ComponentMetrics {
  uint64_t events_processed = 0;  ///< Total events processed
  uint64_t bytes_transferred = 0; ///< Total bytes transferred
  uint32_t queue_size = 0;        ///< Current queue size
  uint32_t queue_max = 0;         ///< Maximum queue capacity
  double event_rate = 0.0;        ///< Events per second
  double data_rate = 0.0;         ///< Data rate in MB/s
};

/**
 * @brief Status information for a component
 *
 * Sent periodically from components to the operator for monitoring.
 */
struct ComponentStatus {
  std::string component_id;      ///< Unique component identifier
  ComponentState state;          ///< Current state
  uint64_t timestamp;            ///< Unix timestamp in milliseconds
  uint32_t run_number;           ///< Current run number (0 if not running)
  ComponentMetrics metrics;      ///< Performance metrics
  std::string error_message;     ///< Error description (empty if no error)
  uint64_t heartbeat_counter;    ///< Incremented each status report
};

} // namespace DELILA

#endif // DELILA_CORE_COMPONENT_STATUS_HPP
