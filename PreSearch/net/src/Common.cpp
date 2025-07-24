#include "Common.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>

namespace delila {
namespace test {

std::string TransportTypeToString(TransportType type) {
    switch (type) {
        case TransportType::GRPC: return "grpc";
        case TransportType::ZEROMQ: return "zeromq";
        default: return "unknown";
    }
}

std::string NetworkTypeToString(NetworkType type) {
    switch (type) {
        case NetworkType::TCP: return "tcp";
        case NetworkType::INPROC: return "inproc";
        default: return "unknown";
    }
}

std::string ComponentTypeToString(ComponentType type) {
    switch (type) {
        case ComponentType::DATA_SENDER: return "data_sender";
        case ComponentType::DATA_HUB: return "data_hub";
        case ComponentType::DATA_RECEIVER: return "data_receiver";
        case ComponentType::TEST_RUNNER: return "test_runner";
        default: return "unknown";
    }
}

TransportType StringToTransportType(const std::string& str) {
    if (str == "grpc") return TransportType::GRPC;
    if (str == "zeromq") return TransportType::ZEROMQ;
    return TransportType::GRPC; // default
}

NetworkType StringToNetworkType(const std::string& str) {
    if (str == "tcp") return NetworkType::TCP;
    if (str == "inproc") return NetworkType::INPROC;
    return NetworkType::TCP; // default
}

double GetSystemMemoryUsage() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return 0.0;
    }
    
    uint64_t memTotal = 0;
    uint64_t memAvailable = 0;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            memTotal = std::stoull(value);
        } else if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            memAvailable = std::stoull(value);
        }
    }
    
    if (memTotal == 0) return 0.0;
    return static_cast<double>(memTotal - memAvailable) / memTotal;
}

double GetSystemCpuUsage() {
    static long lastTotalTime = 0;
    static long lastIdleTime = 0;
    
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return 0.0;
    }
    
    std::string line;
    std::getline(file, line);
    
    std::istringstream iss(line);
    std::string cpu;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    
    long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;
    long idleTime = idle + iowait;
    
    if (lastTotalTime == 0) {
        lastTotalTime = totalTime;
        lastIdleTime = idleTime;
        return 0.0;
    }
    
    long totalDiff = totalTime - lastTotalTime;
    long idleDiff = idleTime - lastIdleTime;
    
    lastTotalTime = totalTime;
    lastIdleTime = idleTime;
    
    if (totalDiff == 0) return 0.0;
    return static_cast<double>(totalDiff - idleDiff) / totalDiff;
}

bool IsMemoryUsageHigh(double threshold) {
    return GetSystemMemoryUsage() > threshold;
}

uint64_t GetCurrentTimestampNs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

double GetElapsedMs(TimePoint start, TimePoint end) {
    auto duration = end - start;
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
}

}} // namespace delila::test