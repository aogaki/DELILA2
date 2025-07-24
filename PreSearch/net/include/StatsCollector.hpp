#pragma once

#include "Common.hpp"
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>

namespace delila {
namespace test {

// Statistics collector
class StatsCollector {
public:
    // Constructor
    StatsCollector();
    
    // Destructor
    ~StatsCollector();
    
    // Record message reception
    void RecordMessage(size_t bytes, double latencyUs = 0.0);
    
    // Record system metrics
    void RecordSystemMetrics(double cpuUsage, double memoryUsage);
    
    // Start timing
    void Start();
    
    // Stop timing
    void Stop();
    
    // Reset statistics
    void Reset();
    
    // Generate report
    StatsReport GenerateReport() const;
    
    // Real-time statistics
    double GetCurrentThroughputMBps() const;
    double GetCurrentThroughputMsgsPerSec() const;
    uint64_t GetTotalMessages() const;
    uint64_t GetTotalBytes() const;
    
    // Latency statistics
    double GetLatencyMean() const;
    double GetLatencyPercentile(double percentile) const;
    
private:
    // Atomic counters
    std::atomic<uint64_t> messagesReceived;
    std::atomic<uint64_t> bytesReceived;
    std::atomic<bool> isRunning;
    
    // Timing
    TimePoint startTime;
    TimePoint endTime;
    
    // Latency tracking
    mutable std::mutex latencyMutex;
    std::vector<double> latencies;
    
    // System metrics
    mutable std::mutex systemMetricsMutex;
    std::vector<double> cpuUsageHistory;
    std::vector<double> memoryUsageHistory;
    
    // Helper methods
    void UpdateLatencyStats();
    double CalculatePercentile(const std::vector<double>& data, double percentile) const;
};

}} // namespace delila::test