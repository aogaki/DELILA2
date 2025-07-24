#include "MemoryMonitor.hpp"
#include <chrono>
#include <thread>

namespace delila {
namespace test {

MemoryMonitor::MemoryMonitor() 
    : isRunning(false), currentMemoryUsage(0.0), currentCpuUsage(0.0), 
      memoryThreshold(DEFAULT_MEMORY_THRESHOLD) {
}

MemoryMonitor::~MemoryMonitor() {
    Stop();
}

void MemoryMonitor::Start() {
    if (isRunning.load()) {
        return;
    }
    
    isRunning.store(true);
    monitoringThread = std::make_unique<std::thread>(&MemoryMonitor::MonitoringLoop, this);
}

void MemoryMonitor::Stop() {
    if (!isRunning.load()) {
        return;
    }
    
    isRunning.store(false);
    
    if (monitoringThread && monitoringThread->joinable()) {
        monitoringThread->join();
    }
    
    monitoringThread.reset();
}

bool MemoryMonitor::IsMemoryUsageHigh() const {
    return currentMemoryUsage.load() > memoryThreshold.load();
}

double MemoryMonitor::GetCurrentMemoryUsage() const {
    return currentMemoryUsage.load();
}

double MemoryMonitor::GetCurrentCpuUsage() const {
    return currentCpuUsage.load();
}

void MemoryMonitor::SetMemoryThreshold(double threshold) {
    if (threshold >= 0.0 && threshold <= 1.0) {
        memoryThreshold.store(threshold);
    }
}

double MemoryMonitor::GetMemoryThreshold() const {
    return memoryThreshold.load();
}

bool MemoryMonitor::IsMonitoring() const {
    return isRunning.load();
}

void MemoryMonitor::MonitoringLoop() {
    while (isRunning.load()) {
        // Update memory usage
        double memUsage = GetSystemMemoryUsage();
        currentMemoryUsage.store(memUsage);
        
        // Update CPU usage
        double cpuUsage = GetSystemCpuUsage();
        currentCpuUsage.store(cpuUsage);
        
        // Sleep for monitoring interval
        std::this_thread::sleep_for(std::chrono::milliseconds(MONITORING_INTERVAL_MS));
    }
}

}} // namespace delila::test