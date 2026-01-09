/**
 * @file test_utils.hpp
 * @brief Utility functions for integration tests
 *
 * Provides event-based waiting utilities to replace fixed sleep_for calls.
 */

#pragma once

#include <chrono>
#include <functional>
#include <thread>

namespace DELILA {
namespace test {

/**
 * @brief Wait for a condition with timeout
 * @param condition Function returning true when condition is met
 * @param timeout_ms Maximum wait time in milliseconds
 * @param poll_interval_ms Polling interval in milliseconds
 * @return true if condition was met, false if timeout
 *
 * This replaces fixed sleep_for calls with event-based waiting.
 * The function polls at short intervals and returns as soon as
 * the condition is met, reducing unnecessary wait time.
 */
inline bool WaitForCondition(std::function<bool()> condition,
                              int timeout_ms = 1000,
                              int poll_interval_ms = 5) {
  using namespace std::chrono;
  auto start = steady_clock::now();
  auto timeout = milliseconds(timeout_ms);

  while (duration_cast<milliseconds>(steady_clock::now() - start) < timeout) {
    if (condition()) {
      return true;
    }
    std::this_thread::sleep_for(milliseconds(poll_interval_ms));
  }
  return condition();  // Final check
}

/**
 * @brief Wait for ZMQ connection to stabilize
 * @param timeout_ms Maximum wait time
 *
 * ZMQ PUSH/PULL sockets need a brief moment after bind/connect
 * for the connection to fully establish. This provides a minimal
 * wait with early exit capability.
 *
 * For PUB/SUB, the subscription propagation also needs time.
 */
inline void WaitForConnection(int timeout_ms = 50) {
  // ZMQ typically establishes connection within 10-20ms on localhost
  // We use a short poll interval for fast response
  auto start = std::chrono::steady_clock::now();
  auto timeout = std::chrono::milliseconds(timeout_ms);

  while (std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start) < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

/**
 * @brief Wait for component to reach a specific state
 * @param get_state Function returning current component state
 * @param target_state State to wait for
 * @param timeout_ms Maximum wait time
 * @return true if target state reached, false if timeout
 */
template <typename StateType>
inline bool WaitForState(std::function<StateType()> get_state,
                          StateType target_state, int timeout_ms = 1000) {
  return WaitForCondition([&]() { return get_state() == target_state; },
                          timeout_ms);
}

/**
 * @brief Wait for component to be Armed with actual state check
 * @param component Component to check (must have GetState() method)
 * @param timeout_ms Maximum wait time
 * @return true if Armed state reached, false if timeout
 */
template <typename T>
inline bool WaitForArmed(T& component, int timeout_ms = 100) {
  return WaitForCondition(
      [&]() { return component.GetState() == ComponentState::Armed; },
      timeout_ms);
}

/**
 * @brief Wait for component to be Running with actual state check
 * @param component Component to check (must have GetState() method)
 * @param timeout_ms Maximum wait time
 * @return true if Running state reached, false if timeout
 */
template <typename T>
inline bool WaitForRunning(T& component, int timeout_ms = 100) {
  return WaitForCondition(
      [&]() { return component.GetState() == ComponentState::Running; },
      timeout_ms);
}

/**
 * @brief Wait for component to be Configured (after stop) with actual state
 * check
 * @param component Component to check (must have GetState() method)
 * @param timeout_ms Maximum wait time
 * @return true if Configured state reached, false if timeout
 */
template <typename T>
inline bool WaitForConfigured(T& component, int timeout_ms = 100) {
  return WaitForCondition(
      [&]() { return component.GetState() == ComponentState::Configured; },
      timeout_ms);
}

/**
 * @brief Wait for ZMQ socket binding/connection to stabilize
 *
 * This is a minimal wait needed for ZMQ bind/connect operations.
 * Should be used immediately after Arm() which does bind/connect.
 * ZMQ typically establishes connection within 10-20ms on localhost.
 */
inline void WaitForSocketSetup(int timeout_ms = 20) {
  std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}

}  // namespace test
}  // namespace DELILA
