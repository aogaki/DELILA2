#ifndef DELILA_CORE_OPERATOR_CONFIG_HPP
#define DELILA_CORE_OPERATOR_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace DELILA {

struct ComponentAddress {
  std::string component_id;
  std::string command_address;
  std::string status_address;
  std::string component_type;
  uint32_t start_order = 0;
};

struct OperatorConfig {
  std::string operator_id;
  std::vector<ComponentAddress> components;
  uint32_t configure_timeout_ms = 0;
  uint32_t arm_timeout_ms = 0;
  uint32_t start_timeout_ms = 0;
  uint32_t stop_timeout_ms = 0;
  uint32_t command_retry_count = 0;
  uint32_t command_retry_interval_ms = 0;
};

} // namespace DELILA

#endif // DELILA_CORE_OPERATOR_CONFIG_HPP
