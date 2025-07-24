#pragma once

#include "StatsCollector.hpp"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <map>

namespace delila {
namespace test {

struct TestResult {
    std::string protocol;          // "gRPC" or "ZeroMQ"
    std::string transport;         // "TCP" or "InProc"
    uint32_t batchSize;           // Number of events per batch
    uint64_t totalEvents;         // Total events processed
    uint64_t totalBytes;          // Total bytes processed
    double durationSeconds;       // Test duration in seconds
    double throughputMBps;        // Throughput in MB/s
    double messageRate;           // Messages per second
    
    // Latency statistics
    double meanLatencyMs;         // Mean latency in milliseconds
    double minLatencyMs;          // Minimum latency
    double maxLatencyMs;          // Maximum latency
    double medianLatencyMs;       // 50th percentile (median)
    double p90LatencyMs;          // 90th percentile
    double p99LatencyMs;          // 99th percentile
    
    // Resource usage
    double avgCpuUsage;           // Average CPU usage %
    double peakCpuUsage;          // Peak CPU usage %
    double avgMemoryUsage;        // Average memory usage %
    double peakMemoryUsage;       // Peak memory usage %
    
    // Error statistics
    uint64_t errorCount;          // Number of errors
    double errorRate;             // Error rate %
    
    std::string timestamp;        // Test timestamp
};

class HtmlReportGenerator {
public:
    HtmlReportGenerator();
    ~HtmlReportGenerator();
    
    // Add test result data
    void AddTestResult(const TestResult& result);
    
    // Generate complete HTML report
    bool GenerateReport(const std::string& outputPath);
    
    // Generate report from JSON files
    bool GenerateReportFromJson(const std::string& jsonDir, const std::string& outputPath);
    
private:
    std::vector<TestResult> results_;
    
    // HTML generation methods
    std::string GenerateHtmlHeader() const;
    std::string GenerateHtmlFooter() const;
    std::string GenerateOverviewSection() const;
    std::string GenerateThroughputSection() const;
    std::string GenerateLatencySection() const;
    std::string GenerateResourceUsageSection() const;
    std::string GenerateComparisonSection() const;
    std::string GenerateDetailedResultsSection() const;
    
    // Chart generation methods
    std::string GenerateThroughputChart() const;
    std::string GenerateLatencyChart() const;
    std::string GenerateResourceUsageChart() const;
    std::string GenerateComparisonChart() const;
    
    // Utility methods
    std::string FormatNumber(double value, int decimals = 2) const;
    std::string FormatBytes(uint64_t bytes) const;
    std::string FormatDuration(double seconds) const;
    std::string GetProtocolColor(const std::string& protocol) const;
    
    // Data analysis methods
    std::map<std::string, std::vector<TestResult>> GroupByProtocol() const;
    std::pair<TestResult, TestResult> GetBestAndWorstResults() const;
    double GetAverageImprovement(const std::string& protocol1, const std::string& protocol2) const;
    
    // JSON parsing methods
    bool LoadJsonResults(const std::string& jsonDir);
    TestResult ParseJsonResult(const std::string& jsonFile) const;
};

}} // namespace delila::test