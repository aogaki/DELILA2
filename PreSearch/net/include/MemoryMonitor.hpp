#pragma once

#include "Common.hpp"
#include <atomic>
#include <memory>
#include <thread>

namespace delila {
namespace test {

class MemoryMonitor {
public:
    // Constructor
    MemoryMonitor();
    
    // Destructor
    ~MemoryMonitor();
    
    // Start monitoring
    void Start();
    
    // Stop monitoring
    void Stop();
    
    // Check if memory usage is high
    bool IsMemoryUsageHigh() const;
    
    // Get current memory usage
    double GetCurrentMemoryUsage() const;
    
    // Get current CPU usage
    double GetCurrentCpuUsage() const;
    
    // Set memory threshold (0.0 to 1.0)
    void SetMemoryThreshold(double threshold);
    
    // Get memory threshold
    double GetMemoryThreshold() const;
    
    // Check if monitoring is active
    bool IsMonitoring() const;
    
private:
    void MonitoringLoop();
    
    std::atomic<bool> isRunning;
    std::atomic<double> currentMemoryUsage;
    std::atomic<double> currentCpuUsage;
    std::atomic<double> memoryThreshold;
    
    std::unique_ptr<std::thread> monitoringThread;
    
    static constexpr double DEFAULT_MEMORY_THRESHOLD = 0.8; // 80%
    static constexpr int MONITORING_INTERVAL_MS = 1000; // 1 second
};

}} // namespace delila::test