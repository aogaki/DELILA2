# DELILA2 Network Library Manual

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Core Components](#core-components)
4. [Use Cases with Examples](#use-cases-with-examples)
5. [Configuration](#configuration)
6. [Performance Optimization](#performance-optimization)
7. [Error Handling](#error-handling)
8. [API Reference](#api-reference)

## Overview

The DELILA2 Network Library provides high-performance data transport for nuclear physics data acquisition systems. It consists of two main components:

- **Serializer**: Binary serialization with LZ4 compression and xxHash validation
- **ZMQTransport**: ZeroMQ-based transport with multiple communication patterns

### Key Features

- High-throughput event data streaming
- Automatic compression and checksum validation
- Multiple ZeroMQ patterns (PUB/SUB, PUSH/PULL, REQ/REP)
- JSON-based configuration
- Memory pool optimization
- Zero-copy mode support

## Quick Start

### Building the Library

```bash
# Dependencies
# macOS (Homebrew)
brew install zeromq lz4 xxhash

# Ubuntu/Debian
sudo apt-get install libzmq3-dev liblz4-dev libxxhash-dev

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Running Examples

```bash
# Build examples
cd examples
cmake .
make

# Run basic publisher/subscriber
./pubsub_publisher &
./pubsub_subscriber
```

## Core Components

### EventData Structure

The fundamental data structure representing digitizer events:

```cpp
#include "EventData.hpp"
using namespace DELILA::Digitizer;

// Create event with waveform
auto event = std::make_unique<EventData>(1024);
event->timeStampNs = 1234567890;
event->energy = 1500;
event->module = 0;
event->channel = 3;
event->flags = EventData::FLAG_PILEUP;
```

### Serializer

Handles binary serialization with compression and validation:

```cpp
#include "Serializer.hpp"
using namespace DELILA::Net;

Serializer serializer;
serializer.EnableCompression(true);
serializer.EnableChecksum(true);

// Serialize events
auto encoded = serializer.Encode(events, sequenceNumber);

// Deserialize
auto [decoded_events, seq] = serializer.Decode(encoded);
```

### ZMQTransport

Manages network communication with various patterns:

```cpp
#include "ZMQTransport.hpp"
using namespace DELILA::Net;

ZMQTransport transport;
TransportConfig config;
config.data_address = "tcp://localhost:5555";
config.data_pattern = "PUB";
config.bind_data = true;

transport.Configure(config);
transport.Connect();
```

## Use Cases with Examples

### 1. Basic Publisher/Subscriber Pattern

**Publisher** (examples/pubsub_publisher.cpp):
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // Configure publisher
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5555";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "Publisher started, sending events..." << std::endl;
    
    // Send events continuously
    uint64_t event_number = 0;
    while (true) {
        // Create batch of events
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int i = 0; i < 10; ++i) {
            auto event = std::make_unique<EventData>(512);
            event->timeStampNs = std::chrono::system_clock::now().time_since_epoch().count();
            event->energy = 1000 + (event_number % 2000);
            event->module = 0;
            event->channel = i;
            
            // Fill some waveform data
            for (size_t j = 0; j < 512; ++j) {
                event->analogProbe1[j] = static_cast<int32_t>(1000 * sin(j * 0.1));
            }
            
            events->push_back(std::move(event));
            event_number++;
        }
        
        if (!transport.Send(events)) {
            std::cerr << "Failed to send events" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
```

**Subscriber** (examples/pubsub_subscriber.cpp):
```cpp
#include <iostream>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // Configure subscriber
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5555";
    config.data_pattern = "SUB";
    config.bind_data = false;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "Subscriber started, waiting for events..." << std::endl;
    
    uint64_t total_events = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto [events, sequence] = transport.Receive();
        
        if (events) {
            total_events += events->size();
            
            // Print first event details
            if (!events->empty()) {
                const auto& first_event = (*events)[0];
                std::cout << "Received " << events->size() << " events, sequence: " << sequence
                          << ", first event - energy: " << first_event->energy
                          << ", channel: " << static_cast<int>(first_event->channel) << std::endl;
            }
            
            // Calculate rate every 100 batches
            if (sequence % 100 == 0) {
                auto current_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
                double rate = total_events / (duration.count() + 1.0);
                std::cout << "Event rate: " << rate << " events/sec" << std::endl;
            }
        }
    }
    
    return 0;
}
```

### 2. Push/Pull Pattern for Load Balancing

**Pusher** (examples/push_pull_pusher.cpp):
```cpp
#include <iostream>
#include <thread>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // Configure pusher
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5556";
    config.data_pattern = "PUSH";
    config.bind_data = true;
    
    transport.Configure(config);
    transport.Connect();
    
    std::cout << "Pusher started, distributing work..." << std::endl;
    
    for (int batch = 0; batch < 1000; ++batch) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        // Create work batch
        for (int i = 0; i < 100; ++i) {
            auto event = std::make_unique<EventData>();
            event->energy = batch * 100 + i;
            event->module = batch % 4;
            events->push_back(std::move(event));
        }
        
        transport.Send(events);
        std::cout << "Sent batch " << batch << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

**Puller** (examples/push_pull_puller.cpp):
```cpp
#include <iostream>
#include <thread>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main(int argc, char* argv[]) {
    int worker_id = argc > 1 ? std::stoi(argv[1]) : 0;
    
    // Configure puller
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5556";
    config.data_pattern = "PULL";
    config.bind_data = false;
    
    transport.Configure(config);
    transport.Connect();
    
    std::cout << "Worker " << worker_id << " started, waiting for work..." << std::endl;
    
    while (true) {
        auto [events, sequence] = transport.Receive();
        
        if (events) {
            // Process events
            std::cout << "Worker " << worker_id << " processing " 
                      << events->size() << " events from batch " << sequence << std::endl;
            
            // Simulate processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    return 0;
}
```

### 3. Command/Control with REQ/REP Pattern

**Command Server** (examples/command_server.cpp):
```cpp
#include <iostream>
#include <map>
#include <functional>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

class CommandServer {
private:
    ZMQTransport transport;
    std::map<CommandType, std::function<CommandResponse(const Command&)>> handlers;
    bool running = false;
    
public:
    CommandServer() {
        // Register command handlers
        handlers[CommandType::START] = [this](const Command& cmd) {
            return handleStart(cmd);
        };
        
        handlers[CommandType::STOP] = [this](const Command& cmd) {
            return handleStop(cmd);
        };
        
        handlers[CommandType::CONFIGURE] = [this](const Command& cmd) {
            return handleConfigure(cmd);
        };
    }
    
    bool Initialize(const std::string& address = "tcp://*:5557") {
        TransportConfig config;
        config.command_address = address;
        config.server_mode = true;
        config.command_server = true;
        
        if (!transport.Configure(config)) {
            return false;
        }
        
        return transport.StartServer();
    }
    
    void Run() {
        std::cout << "Command server running..." << std::endl;
        running = true;
        
        while (running) {
            auto command = transport.WaitForCommand(1000);
            
            if (command) {
                std::cout << "Received command: " << static_cast<int>(command->type) 
                          << " for " << command->target_component << std::endl;
                
                CommandResponse response;
                response.command_id = command->command_id;
                response.timestamp = std::chrono::system_clock::now();
                
                // Process command
                if (handlers.find(command->type) != handlers.end()) {
                    response = handlers[command->type](*command);
                } else {
                    response.success = false;
                    response.message = "Unknown command type";
                }
                
                transport.SendCommandResponse(response);
            }
        }
    }
    
private:
    CommandResponse handleStart(const Command& cmd) {
        CommandResponse response;
        response.command_id = cmd.command_id;
        response.success = true;
        response.message = "System started successfully";
        response.timestamp = std::chrono::system_clock::now();
        return response;
    }
    
    CommandResponse handleStop(const Command& cmd) {
        CommandResponse response;
        response.command_id = cmd.command_id;
        response.success = true;
        response.message = "System stopped";
        response.timestamp = std::chrono::system_clock::now();
        running = false;
        return response;
    }
    
    CommandResponse handleConfigure(const Command& cmd) {
        CommandResponse response;
        response.command_id = cmd.command_id;
        response.success = true;
        response.message = "Configuration applied";
        response.timestamp = std::chrono::system_clock::now();
        
        // Echo back configuration parameters
        response.result_data = cmd.parameters;
        
        return response;
    }
};

int main() {
    CommandServer server;
    
    if (!server.Initialize()) {
        std::cerr << "Failed to initialize command server" << std::endl;
        return 1;
    }
    
    server.Run();
    return 0;
}
```

**Command Client** (examples/command_client.cpp):
```cpp
#include <iostream>
#include <chrono>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    // Configure client
    ZMQTransport transport;
    TransportConfig config;
    config.command_address = "tcp://localhost:5557";
    config.server_mode = false;
    config.bind_command = false;
    
    if (!transport.Configure(config)) {
        std::cerr << "Failed to configure transport" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Send START command
    Command start_cmd;
    start_cmd.command_id = "cmd_001";
    start_cmd.type = CommandType::START;
    start_cmd.target_component = "digitizer_01";
    start_cmd.timestamp = std::chrono::system_clock::now();
    
    std::cout << "Sending START command..." << std::endl;
    auto response = transport.SendCommand(start_cmd);
    
    if (response.success) {
        std::cout << "Response: " << response.message << std::endl;
    } else {
        std::cerr << "Command failed: " << response.message << std::endl;
    }
    
    // Send CONFIGURE command
    Command config_cmd;
    config_cmd.command_id = "cmd_002";
    config_cmd.type = CommandType::CONFIGURE;
    config_cmd.target_component = "digitizer_01";
    config_cmd.parameters["sample_rate"] = "1000000";
    config_cmd.parameters["channel_mask"] = "0xFF";
    config_cmd.timestamp = std::chrono::system_clock::now();
    
    std::cout << "Sending CONFIGURE command..." << std::endl;
    response = transport.SendCommand(config_cmd);
    
    if (response.success) {
        std::cout << "Response: " << response.message << std::endl;
        for (const auto& [key, value] : response.result_data) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
    
    // Send STOP command
    Command stop_cmd;
    stop_cmd.command_id = "cmd_003";
    stop_cmd.type = CommandType::STOP;
    stop_cmd.target_component = "digitizer_01";
    stop_cmd.timestamp = std::chrono::system_clock::now();
    
    std::cout << "Sending STOP command..." << std::endl;
    response = transport.SendCommand(stop_cmd);
    std::cout << "Response: " << response.message << std::endl;
    
    return 0;
}
```

### 4. Status Monitoring

**Status Reporter** (examples/status_reporter.cpp):
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    // Configure status publisher
    ZMQTransport transport;
    TransportConfig config;
    config.status_address = "tcp://*:5556";
    config.bind_status = true;
    
    transport.Configure(config);
    transport.Connect();
    
    std::cout << "Status reporter started..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> temp_dist(20.0, 30.0);
    std::uniform_real_distribution<> rate_dist(0.8, 1.2);
    
    uint64_t heartbeat = 0;
    
    while (true) {
        ComponentStatus status;
        status.component_id = "digitizer_01";
        status.state = "RUNNING";
        status.timestamp = std::chrono::system_clock::now();
        status.heartbeat_counter = heartbeat++;
        
        // Add metrics
        status.metrics["temperature_c"] = temp_dist(gen);
        status.metrics["event_rate_hz"] = 1000000 * rate_dist(gen);
        status.metrics["buffer_usage_percent"] = 25.5 + (heartbeat % 50);
        status.metrics["dropped_events"] = 0;
        
        if (!transport.SendStatus(status)) {
            std::cerr << "Failed to send status" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

**Status Monitor** (examples/status_monitor.cpp):
```cpp
#include <iostream>
#include <iomanip>
#include <chrono>
#include <map>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;

int main() {
    // Configure status subscriber
    ZMQTransport transport;
    TransportConfig config;
    config.status_address = "tcp://localhost:5556";
    config.bind_status = false;
    
    transport.Configure(config);
    transport.Connect();
    
    std::cout << "Status monitor started..." << std::endl;
    
    std::map<std::string, ComponentStatus> component_status;
    
    while (true) {
        auto status = transport.ReceiveStatus();
        
        if (status) {
            component_status[status->component_id] = *status;
            
            // Clear screen and display all component statuses
            std::cout << "\033[2J\033[H"; // Clear screen
            std::cout << "=== System Status Monitor ===" << std::endl;
            std::cout << std::endl;
            
            for (const auto& [id, st] : component_status) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - st.timestamp
                ).count();
                
                std::cout << "Component: " << id << std::endl;
                std::cout << "  State: " << st.state;
                std::cout << " (updated " << age << "s ago)" << std::endl;
                std::cout << "  Heartbeat: " << st.heartbeat_counter << std::endl;
                
                if (!st.error_message.empty()) {
                    std::cout << "  ERROR: " << st.error_message << std::endl;
                }
                
                std::cout << "  Metrics:" << std::endl;
                for (const auto& [metric, value] : st.metrics) {
                    std::cout << "    " << std::setw(25) << std::left << metric 
                              << ": " << std::fixed << std::setprecision(2) << value << std::endl;
                }
                std::cout << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
```

### 5. JSON Configuration

**Configuration Example** (examples/config_example.cpp):
```cpp
#include <iostream>
#include <fstream>
#include "ZMQTransport.hpp"
#include "nlohmann/json.hpp"

using namespace DELILA::Net;
using json = nlohmann::json;

int main() {
    // Example 1: Configure from JSON object
    ZMQTransport transport1;
    
    json config = {
        {"transport", {
            {"data_channel", {
                {"address", "tcp://localhost:5555"},
                {"pattern", "PUB"},
                {"bind", true}
            }},
            {"status_channel", {
                {"address", "tcp://localhost:5556"},
                {"bind", true}
            }},
            {"command_channel", {
                {"address", "tcp://localhost:5557"},
                {"bind", false}
            }}
        }}
    };
    
    if (transport1.ConfigureFromJSON(config)) {
        std::cout << "Configured from JSON object successfully" << std::endl;
    }
    
    // Example 2: Configure from file
    ZMQTransport transport2;
    
    // Create example config file
    std::ofstream config_file("example_config.json");
    config_file << config.dump(4);
    config_file.close();
    
    if (transport2.ConfigureFromFile("example_config.json")) {
        std::cout << "Configured from file successfully" << std::endl;
    }
    
    // Example 3: DAQ role-based configuration
    json daq_config = {
        {"daq_role", "frontend"},
        {"component_id", "digitizer_01"},
        {"endpoints", {
            {"data_sink", "tcp://daq-server:5555"},
            {"control", "tcp://daq-server:5557"}
        }}
    };
    
    ZMQTransport transport3;
    if (transport3.ConfigureFromJSON(daq_config)) {
        std::cout << "Configured with DAQ role successfully" << std::endl;
    }
    
    return 0;
}
```

### 6. Performance Testing

**Throughput Test** (examples/throughput_test.cpp):
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

std::atomic<uint64_t> total_bytes{0};
std::atomic<uint64_t> total_events{0};

void publisher_thread(int batch_size, int event_size) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5558";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    transport.Configure(config);
    transport.Connect();
    
    // Enable optimizations
    transport.EnableZeroCopy(true);
    transport.EnableMemoryPool(true);
    transport.SetMemoryPoolSize(50);
    
    while (true) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int i = 0; i < batch_size; ++i) {
            auto event = std::make_unique<EventData>(event_size);
            event->timeStampNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            event->energy = i;
            events->push_back(std::move(event));
        }
        
        if (transport.Send(events)) {
            total_events += batch_size;
            total_bytes += batch_size * (EVENTDATA_SIZE + event_size * sizeof(int32_t) * 2);
        }
    }
}

void subscriber_thread() {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5558";
    config.data_pattern = "SUB";
    config.bind_data = false;
    
    transport.Configure(config);
    transport.Connect();
    
    while (true) {
        auto [events, sequence] = transport.Receive();
        // Just receive and discard
    }
}

int main() {
    const int batch_size = 100;
    const int event_size = 1024;
    
    std::cout << "Starting throughput test..." << std::endl;
    std::cout << "Batch size: " << batch_size << " events" << std::endl;
    std::cout << "Event size: " << event_size << " samples" << std::endl;
    
    // Start threads
    std::thread pub_thread(publisher_thread, batch_size, event_size);
    std::thread sub_thread(subscriber_thread);
    
    // Monitor performance
    auto start_time = std::chrono::steady_clock::now();
    uint64_t last_events = 0;
    uint64_t last_bytes = 0;
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        uint64_t current_events = total_events.load();
        uint64_t current_bytes = total_bytes.load();
        
        uint64_t events_diff = current_events - last_events;
        uint64_t bytes_diff = current_bytes - last_bytes;
        
        double mbps = (bytes_diff / 1024.0 / 1024.0);
        
        std::cout << "Events/sec: " << events_diff 
                  << ", MB/sec: " << std::fixed << std::setprecision(2) << mbps
                  << ", Total events: " << current_events << std::endl;
        
        last_events = current_events;
        last_bytes = current_bytes;
    }
    
    pub_thread.join();
    sub_thread.join();
    
    return 0;
}
```

### 7. Memory Pool Optimization

**Memory Pool Example** (examples/memory_pool_example.cpp):
```cpp
#include <iostream>
#include <chrono>
#include <vector>
#include "ZMQTransport.hpp"
#include "Serializer.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

void benchmark_with_pool(bool use_pool, int iterations) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5559";
    config.data_pattern = "PUB";
    config.bind_data = true;
    
    transport.Configure(config);
    transport.Connect();
    
    // Configure memory pool
    transport.EnableMemoryPool(use_pool);
    if (use_pool) {
        transport.SetMemoryPoolSize(20);
    }
    
    // Also configure serializer buffer pool
    Serializer serializer;
    serializer.EnableBufferPool(use_pool);
    if (use_pool) {
        serializer.SetBufferPoolSize(20);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int j = 0; j < 100; ++j) {
            auto event = std::make_unique<EventData>(1024);
            event->energy = i * 100 + j;
            events->push_back(std::move(event));
        }
        
        transport.Send(events);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << (use_pool ? "With pool: " : "Without pool: ")
              << duration.count() << " ms for " << iterations << " iterations" << std::endl;
    
    if (use_pool) {
        std::cout << "Pooled containers: " << transport.GetPooledContainerCount() << std::endl;
    }
}

int main() {
    std::cout << "Memory Pool Performance Comparison" << std::endl;
    std::cout << "==================================" << std::endl;
    
    const int iterations = 1000;
    
    // Warm up
    benchmark_with_pool(false, 10);
    
    // Benchmark without pool
    benchmark_with_pool(false, iterations);
    
    // Benchmark with pool
    benchmark_with_pool(true, iterations);
    
    return 0;
}
```

### 8. Error Handling and Recovery

**Robust Client** (examples/robust_client.cpp):
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

class RobustDataClient {
private:
    ZMQTransport transport;
    TransportConfig config;
    bool connected = false;
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 5;
    
public:
    RobustDataClient(const std::string& server_address) {
        config.data_address = server_address;
        config.data_pattern = "SUB";
        config.bind_data = false;
    }
    
    bool Connect() {
        if (!transport.Configure(config)) {
            std::cerr << "Failed to configure transport" << std::endl;
            return false;
        }
        
        connected = transport.Connect();
        if (connected) {
            std::cout << "Connected to " << config.data_address << std::endl;
            reconnect_attempts = 0;
        }
        
        return connected;
    }
    
    void Run() {
        while (true) {
            if (!connected) {
                if (!Reconnect()) {
                    std::cerr << "Failed to reconnect after " << max_reconnect_attempts 
                              << " attempts. Exiting." << std::endl;
                    break;
                }
            }
            
            try {
                auto [events, sequence] = transport.Receive();
                
                if (events) {
                    ProcessEvents(events, sequence);
                } else {
                    // No data received, check connection
                    if (!transport.IsConnected()) {
                        std::cerr << "Connection lost" << std::endl;
                        connected = false;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error receiving data: " << e.what() << std::endl;
                connected = false;
            }
        }
    }
    
private:
    bool Reconnect() {
        while (reconnect_attempts < max_reconnect_attempts) {
            reconnect_attempts++;
            std::cout << "Reconnection attempt " << reconnect_attempts 
                      << "/" << max_reconnect_attempts << std::endl;
            
            // Disconnect and wait
            transport.Disconnect();
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_attempts));
            
            // Try to reconnect
            if (Connect()) {
                return true;
            }
        }
        
        return false;
    }
    
    void ProcessEvents(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events,
                       uint64_t sequence) {
        std::cout << "Received " << events->size() << " events, sequence: " << sequence << std::endl;
        
        // Process events here
        for (const auto& event : *events) {
            if (event->HasPileup()) {
                std::cout << "Warning: Event has pileup flag set" << std::endl;
            }
            if (event->HasOverRange()) {
                std::cout << "Warning: Event has over-range flag set" << std::endl;
            }
        }
    }
};

int main() {
    RobustDataClient client("tcp://localhost:5555");
    
    if (!client.Connect()) {
        std::cerr << "Initial connection failed" << std::endl;
        return 1;
    }
    
    client.Run();
    
    return 0;
}
```

## Configuration

### Basic Configuration Structure

```cpp
TransportConfig config;
config.data_address = "tcp://localhost:5555";     // Data channel address
config.status_address = "tcp://localhost:5556";   // Status channel address
config.command_address = "tcp://localhost:5557";  // Command channel address
config.data_pattern = "PUB";                      // PUB, SUB, PUSH, or PULL
config.bind_data = true;                          // Bind (true) or connect (false)
config.bind_status = true;                        // Bind status socket
config.bind_command = false;                      // Connect to command server
```

### JSON Configuration Schema

```json
{
  "transport": {
    "data_channel": {
      "address": "tcp://localhost:5555",
      "pattern": "PUB",
      "bind": true
    },
    "status_channel": {
      "address": "tcp://localhost:5556",
      "bind": true
    },
    "command_channel": {
      "address": "tcp://localhost:5557",
      "bind": false,
      "server_mode": false
    }
  }
}
```

### DAQ Role Configuration

```json
{
  "daq_role": "frontend",
  "component_id": "digitizer_01",
  "endpoints": {
    "data_sink": "tcp://daq-server:5555",
    "control": "tcp://daq-server:5557"
  }
}
```

## Performance Optimization

### 1. Enable Compression

```cpp
transport.EnableCompression(true);  // LZ4 compression
```

### 2. Enable Zero-Copy Mode

```cpp
transport.EnableZeroCopy(true);  // Avoid memory copies
```

### 3. Use Memory Pools

```cpp
transport.EnableMemoryPool(true);
transport.SetMemoryPoolSize(50);  // Pool size
```

### 4. Batch Events

```cpp
// Send multiple events in one batch
auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
for (int i = 0; i < 100; ++i) {
    events->push_back(std::make_unique<EventData>());
}
transport.Send(events);
```

## Error Handling

### Connection Errors

```cpp
if (!transport.Connect()) {
    std::cerr << "Failed to connect: check address and network" << std::endl;
}
```

### Send/Receive Errors

```cpp
if (!transport.Send(events)) {
    std::cerr << "Send failed: check connection" << std::endl;
}

auto [events, sequence] = transport.Receive();
if (!events) {
    std::cerr << "No data received: timeout or connection issue" << std::endl;
}
```

### Configuration Errors

```cpp
try {
    if (!transport.ConfigureFromFile("config.json")) {
        std::cerr << "Invalid configuration" << std::endl;
    }
} catch (const std::exception& e) {
    std::cerr << "Configuration error: " << e.what() << std::endl;
}
```

## API Reference

### ZMQTransport Class

#### Connection Management
- `bool Configure(const TransportConfig& config)`
- `bool ConfigureFromJSON(const nlohmann::json& config)`
- `bool ConfigureFromFile(const std::string& filename)`
- `bool Connect()`
- `void Disconnect()`
- `bool IsConnected() const`

#### Data Transfer
- `bool Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events)`
- `std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> Receive()`

#### Status Communication
- `bool SendStatus(const ComponentStatus& status)`
- `std::unique_ptr<ComponentStatus> ReceiveStatus()`

#### Command Communication
- `CommandResponse SendCommand(const Command& command)`
- `std::unique_ptr<Command> ReceiveCommand()`

#### Server Mode (REP Pattern)
- `bool StartServer()`
- `void StopServer()`
- `bool IsServerRunning() const`
- `std::unique_ptr<Command> WaitForCommand(int timeout_ms = 1000)`
- `bool SendCommandResponse(const CommandResponse& response)`

#### Optimization Controls
- `void EnableCompression(bool enable)`
- `void EnableChecksum(bool enable)`
- `void EnableZeroCopy(bool enable)`
- `void EnableMemoryPool(bool enable)`
- `void SetMemoryPoolSize(size_t size)`

### Serializer Class

#### Configuration
- `void EnableCompression(bool enable)`
- `void EnableChecksum(bool enable)`
- `void EnableBufferPool(bool enable)`
- `void SetBufferPoolSize(size_t size)`

#### Serialization
- `std::unique_ptr<std::vector<uint8_t>> Encode(...)`
- `std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t> Decode(...)`

### Data Structures

#### EventData
- Core event data structure with timestamp, energy, waveforms
- Flag support for pileup, trigger lost, over-range

#### ComponentStatus
- Component health monitoring
- Metrics map for custom values
- Heartbeat counter

#### Command/CommandResponse
- Command types: START, STOP, PAUSE, RESUME, CONFIGURE, RESET, SHUTDOWN
- Parameter map for flexible command data
- Response includes success/failure and result data

## Building and Running Examples

```bash
# Build all examples
cd examples
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run basic pub/sub
./pubsub_publisher &
./pubsub_subscriber

# Run push/pull with multiple workers
./push_pull_pusher &
./push_pull_puller 1 &
./push_pull_puller 2 &
./push_pull_puller 3

# Run command server and client
./command_server &
./command_client

# Run performance tests
./throughput_test
./memory_pool_example

# Run robust client example
./robust_client
```

### Important Configuration Notes

When using ZMQTransport in simple examples, you need to properly configure the socket addresses to avoid conflicts:

1. **For data-only communication** (PUB/SUB, PUSH/PULL):
   ```cpp
   config.status_address = config.data_address;
   config.command_address = config.data_address;
   ```

2. **For command-only servers**:
   ```cpp
   config.data_address = "tcp://localhost:9998";  // Different unused address
   config.status_address = "tcp://localhost:9999";
   config.data_pattern = "SUB";
   config.bind_data = false;
   ```

3. **Status Reporting Note**: The status reporter/monitor examples use REQ/REP pattern which requires a status server to be running. For simple status broadcasting, consider using the data channel with PUB/SUB pattern instead.

## Troubleshooting

### Common Issues

1. **Connection Refused**
   - Check if the server is running
   - Verify the address and port
   - Check firewall settings

2. **High CPU Usage**
   - Enable sleep/wait in receive loops
   - Use appropriate socket timeouts
   - Check for busy-wait loops

3. **Memory Growth**
   - Enable memory pools
   - Check for event accumulation
   - Monitor container sizes

4. **Poor Performance**
   - Enable compression for large events
   - Use batch sending
   - Enable zero-copy mode
   - Tune memory pool sizes

### Debug Tips

1. Enable verbose logging in ZeroMQ
2. Use network monitoring tools (tcpdump, wireshark)
3. Monitor system resources (top, htop)
4. Check ZeroMQ socket statistics

## Best Practices

1. **Always check return values** from Send/Receive operations
2. **Use appropriate patterns** for your use case (PUB/SUB for broadcast, PUSH/PULL for load balancing)
3. **Configure timeouts** to avoid indefinite blocking
4. **Implement reconnection logic** for robust applications
5. **Monitor performance metrics** in production
6. **Use JSON configuration** for flexibility
7. **Enable optimizations** based on your performance requirements
8. **Test error scenarios** including network failures