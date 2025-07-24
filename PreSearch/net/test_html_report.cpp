#include "include/HtmlReportGenerator.hpp"
#include <iostream>

using namespace delila::test;

int main() {
    std::cout << "Testing HTML Report Generator..." << std::endl;
    
    HtmlReportGenerator generator;
    
    // Add test results for ZeroMQ
    TestResult zmqResult1;
    zmqResult1.protocol = "ZeroMQ";
    zmqResult1.transport = "TCP";
    zmqResult1.batchSize = 1024;
    zmqResult1.totalEvents = 85000;
    zmqResult1.totalBytes = 8500000;
    zmqResult1.durationSeconds = 30.0;
    zmqResult1.throughputMBps = 73.87;
    zmqResult1.messageRate = 2000.0;
    zmqResult1.meanLatencyMs = 0.5;
    zmqResult1.minLatencyMs = 0.1;
    zmqResult1.maxLatencyMs = 2.0;
    zmqResult1.medianLatencyMs = 0.4;
    zmqResult1.p90LatencyMs = 0.8;
    zmqResult1.p99LatencyMs = 1.5;
    zmqResult1.avgCpuUsage = 45.0;
    zmqResult1.peakCpuUsage = 78.0;
    zmqResult1.avgMemoryUsage = 32.0;
    zmqResult1.peakMemoryUsage = 55.0;
    zmqResult1.errorCount = 0;
    zmqResult1.errorRate = 0.0;
    zmqResult1.timestamp = "2025-01-17T12:00:00Z";
    
    TestResult zmqResult2;
    zmqResult2.protocol = "ZeroMQ";
    zmqResult2.transport = "TCP";
    zmqResult2.batchSize = 10240;
    zmqResult2.totalEvents = 102400;
    zmqResult2.totalBytes = 10240000;
    zmqResult2.durationSeconds = 30.0;
    zmqResult2.throughputMBps = 95.23;
    zmqResult2.messageRate = 2500.0;
    zmqResult2.meanLatencyMs = 0.6;
    zmqResult2.minLatencyMs = 0.2;
    zmqResult2.maxLatencyMs = 2.5;
    zmqResult2.medianLatencyMs = 0.5;
    zmqResult2.p90LatencyMs = 1.0;
    zmqResult2.p99LatencyMs = 1.8;
    zmqResult2.avgCpuUsage = 52.0;
    zmqResult2.peakCpuUsage = 85.0;
    zmqResult2.avgMemoryUsage = 38.0;
    zmqResult2.peakMemoryUsage = 62.0;
    zmqResult2.errorCount = 0;
    zmqResult2.errorRate = 0.0;
    zmqResult2.timestamp = "2025-01-17T12:30:00Z";
    
    // Add test results for gRPC
    TestResult grpcResult1;
    grpcResult1.protocol = "gRPC";
    grpcResult1.transport = "TCP";
    grpcResult1.batchSize = 1024;
    grpcResult1.totalEvents = 80000;
    grpcResult1.totalBytes = 8000000;
    grpcResult1.durationSeconds = 30.0;
    grpcResult1.throughputMBps = 65.23;
    grpcResult1.messageRate = 1800.0;
    grpcResult1.meanLatencyMs = 0.7;
    grpcResult1.minLatencyMs = 0.2;
    grpcResult1.maxLatencyMs = 3.0;
    grpcResult1.medianLatencyMs = 0.6;
    grpcResult1.p90LatencyMs = 1.2;
    grpcResult1.p99LatencyMs = 2.1;
    grpcResult1.avgCpuUsage = 52.0;
    grpcResult1.peakCpuUsage = 82.0;
    grpcResult1.avgMemoryUsage = 38.0;
    grpcResult1.peakMemoryUsage = 58.0;
    grpcResult1.errorCount = 0;
    grpcResult1.errorRate = 0.0;
    grpcResult1.timestamp = "2025-01-17T13:00:00Z";
    
    TestResult grpcResult2;
    grpcResult2.protocol = "gRPC";
    grpcResult2.transport = "TCP";
    grpcResult2.batchSize = 10240;
    grpcResult2.totalEvents = 98000;
    grpcResult2.totalBytes = 9800000;
    grpcResult2.durationSeconds = 30.0;
    grpcResult2.throughputMBps = 85.67;
    grpcResult2.messageRate = 2200.0;
    grpcResult2.meanLatencyMs = 0.8;
    grpcResult2.minLatencyMs = 0.3;
    grpcResult2.maxLatencyMs = 3.5;
    grpcResult2.medianLatencyMs = 0.7;
    grpcResult2.p90LatencyMs = 1.4;
    grpcResult2.p99LatencyMs = 2.5;
    grpcResult2.avgCpuUsage = 58.0;
    grpcResult2.peakCpuUsage = 88.0;
    grpcResult2.avgMemoryUsage = 42.0;
    grpcResult2.peakMemoryUsage = 65.0;
    grpcResult2.errorCount = 0;
    grpcResult2.errorRate = 0.0;
    grpcResult2.timestamp = "2025-01-17T13:30:00Z";
    
    // Add all results to generator
    generator.AddTestResult(zmqResult1);
    generator.AddTestResult(zmqResult2);
    generator.AddTestResult(grpcResult1);
    generator.AddTestResult(grpcResult2);
    
    // Generate HTML report
    std::string outputPath = "test_performance_report.html";
    if (generator.GenerateReport(outputPath)) {
        std::cout << "✓ HTML report generated successfully: " << outputPath << std::endl;
        std::cout << "  Open the file in your browser to view the report." << std::endl;
        return 0;
    } else {
        std::cout << "✗ Failed to generate HTML report" << std::endl;
        return 1;
    }
}