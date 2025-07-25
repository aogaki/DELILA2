/**
 * @file controller_example.cpp
 * @brief Example of controller module coordinating DAQ system
 * 
 * Demonstrates:
 * - DEALER/ROUTER pattern for asynchronous control
 * - Protocol buffer control messages
 * - Module state management
 * - Status collection from multiple modules
 */

#include <delila/net/delila_net.hpp>
#include <delila/net/proto/control_messages.pb.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <map>

using namespace DELILA::Net;
using namespace delila::net::control;

int main() {
    try {
        // Create control connections
        Connection control_router(Connection::ROUTER);
        Connection status_subscriber(Connection::SUBSCRIBER);
        
        // Bind for module control communication
        auto router_result = control_router.bind("*:5557", Connection::AUTO);
        if (!isOk(router_result)) {
            std::cerr << "Failed to bind router: " << getError(router_result).message << std::endl;
            return 1;
        }
        
        // Subscribe to status reports from all modules
        auto sub_result = status_subscriber.connect("127.0.0.1:5558", Connection::AUTO);
        if (!isOk(sub_result)) {
            std::cerr << "Failed to connect subscriber: " << getError(sub_result).message << std::endl;
            return 1;
        }
        
        // Track module states
        std::map<std::string, ModuleState> module_states;
        std::vector<std::string> modules = {"fetcher1", "fetcher2", "fetcher3", "merger", "recorder"};
        
        std::cout << "Controller started - managing DAQ system..." << std::endl;
        
        // Initialize all modules to BOOTED state
        for (const auto& module : modules) {
            module_states[module] = STATE_BOOTED;
        }
        
        // State transition sequence: BOOTED -> CONFIGURED -> RUNNING
        std::vector<ModuleState> state_sequence = {STATE_CONFIGURED, STATE_RUNNING};
        
        for (ModuleState target_state : state_sequence) {
            std::cout << "\n=== Transitioning all modules to state: " 
                      << ModuleState_Name(target_state) << " ===" << std::endl;
            
            // Send state change commands to all modules
            for (const auto& module : modules) {
                StateChangeCommand command;
                command.set_module_id(module);
                command.set_target_state(target_state);
                command.set_timestamp_ns(std::chrono::high_resolution_clock::now().time_since_epoch().count());
                command.set_command_id("cmd_" + std::to_string(std::time(nullptr)) + "_" + module);
                
                // Serialize protobuf message
                std::string serialized_command;
                command.SerializeToString(&serialized_command);
                
                // Create message with module routing
                Message msg;
                msg.data.assign(serialized_command.begin(), serialized_command.end());
                msg.type = MessageType::CONTROL_STATE;
                
                auto send_result = control_router.send(msg);
                if (isOk(send_result)) {
                    std::cout << "Sent " << ModuleState_Name(target_state) 
                              << " command to " << module << std::endl;
                } else {
                    std::cerr << "Failed to send command to " << module 
                              << ": " << getError(send_result).message << std::endl;
                }
            }
            
            // Wait for responses from all modules
            int responses_received = 0;
            auto start_time = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(10);
            
            while (responses_received < modules.size()) {
                auto now = std::chrono::steady_clock::now();
                if (now - start_time > timeout) {
                    std::cerr << "Timeout waiting for module responses" << std::endl;
                    break;
                }
                
                // Check for responses
                auto receive_result = control_router.receive();
                if (isOk(receive_result)) {
                    const auto& response_msg = getValue(receive_result);
                    
                    // Deserialize response
                    StateChangeResponse response;
                    std::string serialized_data(response_msg.data.begin(), response_msg.data.end());
                    if (response.ParseFromString(serialized_data)) {
                        if (response.success()) {
                            module_states[response.module_id()] = response.current_state();
                            std::cout << "Module " << response.module_id() 
                                      << " transitioned to " << ModuleState_Name(response.current_state()) << std::endl;
                            responses_received++;
                        } else {
                            std::cerr << "Module " << response.module_id() 
                                      << " failed to transition: " << response.error_message() << std::endl;
                        }
                    }
                } else {
                    // No message available, sleep briefly
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            
            // Verify all modules reached target state
            bool all_transitioned = true;
            for (const auto& module : modules) {
                if (module_states[module] != target_state) {
                    std::cerr << "Module " << module << " failed to reach " 
                              << ModuleState_Name(target_state) << std::endl;
                    all_transitioned = false;
                }
            }
            
            if (all_transitioned) {
                std::cout << "All modules successfully transitioned to " 
                          << ModuleState_Name(target_state) << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        // Monitor system status for a while
        std::cout << "\n=== Monitoring system status ===" << std::endl;
        auto monitor_start = std::chrono::steady_clock::now();
        const auto monitor_duration = std::chrono::seconds(30);
        
        while (std::chrono::steady_clock::now() - monitor_start < monitor_duration) {
            auto status_result = status_subscriber.receive();
            if (isOk(status_result)) {
                const auto& status_msg = getValue(status_result);
                
                if (status_msg.type == MessageType::CONTROL_STATUS) {
                    StatusReport status;
                    std::string serialized_data(status_msg.data.begin(), status_msg.data.end());
                    if (status.ParseFromString(serialized_data)) {
                        std::cout << "Status from " << status.module_id() 
                                  << ": Rate=" << status.data_rate_mbps() << " MB/s"
                                  << ", Errors=" << status.error_counter()
                                  << ", Processed=" << status.processed_data_size_bytes() / (1024*1024) << " MB"
                                  << std::endl;
                    }
                } else if (status_msg.type == MessageType::CONTROL_HEARTBEAT) {
                    HeartbeatMessage heartbeat;
                    std::string serialized_data(status_msg.data.begin(), status_msg.data.end());
                    if (heartbeat.ParseFromString(serialized_data)) {
                        std::cout << "Heartbeat from " << heartbeat.module_id() 
                                  << ": " << ModuleState_Name(heartbeat.status()) << std::endl;
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        std::cout << "Controller finished" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}