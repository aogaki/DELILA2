#include "StatsCollector.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace delila {
namespace test {

StatsCollector::StatsCollector() 
    : messagesReceived(0), bytesReceived(0), isRunning(false) {
}

StatsCollector::~StatsCollector() = default;

void StatsCollector::RecordMessage(size_t bytes, double latencyUs) {
    messagesReceived.fetch_add(1);
    bytesReceived.fetch_add(bytes);
    
    if (latencyUs > 0.0) {
        std::lock_guard<std::mutex> lock(latencyMutex);
        latencies.push_back(latencyUs);
    }
}

void StatsCollector::RecordSystemMetrics(double cpuUsage, double memoryUsage) {
    std::lock_guard<std::mutex> lock(systemMetricsMutex);
    cpuUsageHistory.push_back(cpuUsage);
    memoryUsageHistory.push_back(memoryUsage);
    
    // Keep only recent history (last 1000 measurements)
    if (cpuUsageHistory.size() > 1000) {
        cpuUsageHistory.erase(cpuUsageHistory.begin());
    }
    if (memoryUsageHistory.size() > 1000) {
        memoryUsageHistory.erase(memoryUsageHistory.begin());
    }
}

void StatsCollector::Start() {
    isRunning.store(true);
    startTime = std::chrono::steady_clock::now();
}

void StatsCollector::Stop() {
    isRunning.store(false);
    endTime = std::chrono::steady_clock::now();
}

void StatsCollector::Reset() {
    messagesReceived.store(0);
    bytesReceived.store(0);
    isRunning.store(false);
    
    {
        std::lock_guard<std::mutex> lock(latencyMutex);
        latencies.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(systemMetricsMutex);
        cpuUsageHistory.clear();
        memoryUsageHistory.clear();
    }
}

StatsReport StatsCollector::GenerateReport() const {
    StatsReport report;
    
    // Basic counts
    report.messagesReceived = messagesReceived.load();
    report.bytesReceived = bytesReceived.load();
    
    // Time calculation
    TimePoint actualEndTime = isRunning.load() ? std::chrono::steady_clock::now() : endTime;
    auto duration = actualEndTime - startTime;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000000.0;
    
    if (elapsedSeconds > 0.0) {
        report.throughputMBps = (report.bytesReceived / (1024.0 * 1024.0)) / elapsedSeconds;
        report.throughputMsgsPerSec = report.messagesReceived / elapsedSeconds;
    }
    
    // Latency statistics
    {
        std::lock_guard<std::mutex> lock(latencyMutex);
        if (!latencies.empty()) {
            std::vector<double> sortedLatencies = latencies;
            std::sort(sortedLatencies.begin(), sortedLatencies.end());
            
            report.latencyMean = std::accumulate(sortedLatencies.begin(), sortedLatencies.end(), 0.0) / sortedLatencies.size();
            report.latencyMin = sortedLatencies.front();
            report.latencyMax = sortedLatencies.back();
            report.latency50th = CalculatePercentile(sortedLatencies, 0.5);
            report.latency90th = CalculatePercentile(sortedLatencies, 0.9);
            report.latency99th = CalculatePercentile(sortedLatencies, 0.99);
        }
    }
    
    // System metrics
    {
        std::lock_guard<std::mutex> lock(systemMetricsMutex);
        if (!cpuUsageHistory.empty()) {
            report.cpuUsage = std::accumulate(cpuUsageHistory.begin(), cpuUsageHistory.end(), 0.0) / cpuUsageHistory.size();
        }
        if (!memoryUsageHistory.empty()) {
            report.memoryUsage = std::accumulate(memoryUsageHistory.begin(), memoryUsageHistory.end(), 0.0) / memoryUsageHistory.size();
        }
    }
    
    // Metadata
    report.startTime = startTime;
    report.endTime = actualEndTime;
    
    return report;
}

double StatsCollector::GetCurrentThroughputMBps() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - startTime;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000000.0;
    
    if (elapsedSeconds > 0.0) {
        uint64_t bytes = bytesReceived.load();
        return (bytes / (1024.0 * 1024.0)) / elapsedSeconds;
    }
    return 0.0;
}

double StatsCollector::GetCurrentThroughputMsgsPerSec() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - startTime;
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000000.0;
    
    if (elapsedSeconds > 0.0) {
        uint64_t messages = messagesReceived.load();
        return messages / elapsedSeconds;
    }
    return 0.0;
}

uint64_t StatsCollector::GetTotalMessages() const {
    return messagesReceived.load();
}

uint64_t StatsCollector::GetTotalBytes() const {
    return bytesReceived.load();
}

double StatsCollector::GetLatencyMean() const {
    std::lock_guard<std::mutex> lock(latencyMutex);
    if (latencies.empty()) return 0.0;
    
    return std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
}

double StatsCollector::GetLatencyPercentile(double percentile) const {
    std::lock_guard<std::mutex> lock(latencyMutex);
    if (latencies.empty()) return 0.0;
    
    std::vector<double> sortedLatencies = latencies;
    std::sort(sortedLatencies.begin(), sortedLatencies.end());
    
    return CalculatePercentile(sortedLatencies, percentile);
}

double StatsCollector::CalculatePercentile(const std::vector<double>& data, double percentile) const {
    if (data.empty()) return 0.0;
    
    double index = percentile * (data.size() - 1);
    size_t lowerIndex = static_cast<size_t>(std::floor(index));
    size_t upperIndex = static_cast<size_t>(std::ceil(index));
    
    if (lowerIndex == upperIndex) {
        return data[lowerIndex];
    }
    
    double weight = index - lowerIndex;
    return data[lowerIndex] * (1.0 - weight) + data[upperIndex] * weight;
}

}} // namespace delila::test