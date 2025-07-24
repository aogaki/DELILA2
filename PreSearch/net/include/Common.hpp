#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <memory>

namespace delila {
namespace test {

// Forward declarations
class EventDataBatch;
class Config;

// Common types
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

// Transport types
enum class TransportType {
    GRPC,
    ZEROMQ
};

enum class NetworkType {
    TCP,
    INPROC
};

// Component types
enum class ComponentType {
    DATA_SENDER,
    DATA_HUB,
    DATA_RECEIVER,
    TEST_RUNNER
};

// Test scenario
struct TestScenario {
    TransportType protocol;
    NetworkType transport;
    uint32_t batchSize;
    uint32_t durationMinutes;
    std::string outputDir;
};

// Statistics structure
struct StatsReport {
    uint64_t messagesReceived = 0;
    uint64_t bytesReceived = 0;
    double throughputMBps = 0.0;
    double throughputMsgsPerSec = 0.0;
    
    // Latency percentiles (in microseconds)
    double latencyMean = 0.0;
    double latencyMin = 0.0;
    double latencyMax = 0.0;
    double latency50th = 0.0;
    double latency90th = 0.0;
    double latency99th = 0.0;
    
    // System resources
    double cpuUsage = 0.0;
    double memoryUsage = 0.0;
    
    // Test metadata
    std::string protocol;
    uint32_t batchSize = 0;
    uint32_t sourceId = 0;
    TimePoint startTime;
    TimePoint endTime;
};

// Error types
enum class ErrorType {
    NETWORK_FAILURE,
    MEMORY_EXHAUSTION,
    SEQUENCE_ERROR,
    TIMEOUT,
    CONFIGURATION_ERROR
};

// Utility functions
std::string TransportTypeToString(TransportType type);
std::string NetworkTypeToString(NetworkType type);
std::string ComponentTypeToString(ComponentType type);
TransportType StringToTransportType(const std::string& str);
NetworkType StringToNetworkType(const std::string& str);

// Memory and system utilities
double GetSystemMemoryUsage();
double GetSystemCpuUsage();
bool IsMemoryUsageHigh(double threshold = 0.8);

// Time utilities
uint64_t GetCurrentTimestampNs();
double GetElapsedMs(TimePoint start, TimePoint end);

}} // namespace delila::test