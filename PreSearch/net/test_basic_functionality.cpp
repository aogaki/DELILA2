#include "include/StatsCollector.hpp"
#include "include/HtmlReportGenerator.hpp"
#include "include/EventDataBatch.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace delila::test;

void test_stats_collector() {
    std::cout << "Testing StatsCollector..." << std::endl;
    
    StatsCollector stats;
    stats.Start();
    
    // Record some test data
    stats.RecordMessage(1024, 100.0);  // 100 us latency
    stats.RecordMessage(2048, 200.0);  // 200 us latency  
    stats.RecordMessage(1024, 150.0);  // 150 us latency
    stats.RecordMessage(512, 80.0);    // 80 us latency
    stats.RecordMessage(1024, 300.0);  // 300 us latency
    
    // Record system metrics
    stats.RecordSystemMetrics(45.0, 32.0);
    stats.RecordSystemMetrics(50.0, 35.0);
    stats.RecordSystemMetrics(48.0, 33.0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    stats.Stop();
    
    auto report = stats.GenerateReport();
    
    // Verify basic counts
    assert(report.messagesReceived == 5);
    assert(report.bytesReceived == 6656);
    
    // Verify latency calculations
    assert(report.latencyMean > 0);
    assert(report.latency50th > 0);
    assert(report.latency90th > 0);
    assert(report.latency99th > 0);
    assert(report.latencyMin == 80.0);
    assert(report.latencyMax == 300.0);
    
    // Verify system metrics
    assert(report.cpuUsage > 0);
    assert(report.memoryUsage > 0);
    
    std::cout << "  Messages: " << report.messagesReceived << std::endl;
    std::cout << "  Throughput: " << report.throughputMBps << " MB/s" << std::endl;
    std::cout << "  Mean Latency: " << report.latencyMean << " us" << std::endl;
    std::cout << "  P50 Latency: " << report.latency50th << " us" << std::endl;
    std::cout << "  P90 Latency: " << report.latency90th << " us" << std::endl;
    std::cout << "  P99 Latency: " << report.latency99th << " us" << std::endl;
    std::cout << "  CPU Usage: " << report.cpuUsage << "%" << std::endl;
    std::cout << "  Memory Usage: " << report.memoryUsage << "%" << std::endl;
    
    std::cout << "✓ StatsCollector test passed!" << std::endl;
}

void test_html_report_generator() {
    std::cout << "Testing HtmlReportGenerator..." << std::endl;
    
    HtmlReportGenerator generator;
    
    // Add some test results
    TestResult result1;
    result1.protocol = "ZeroMQ";
    result1.batchSize = 1024;
    result1.throughputMBps = 73.87;
    result1.messageRate = 2000.0;
    result1.meanLatencyMs = 0.5;
    result1.p99LatencyMs = 1.5;
    result1.avgCpuUsage = 45.0;
    result1.avgMemoryUsage = 32.0;
    
    TestResult result2;
    result2.protocol = "gRPC";
    result2.batchSize = 1024;
    result2.throughputMBps = 65.23;
    result2.messageRate = 1800.0;
    result2.meanLatencyMs = 0.7;
    result2.p99LatencyMs = 2.1;
    result2.avgCpuUsage = 52.0;
    result2.avgMemoryUsage = 38.0;
    
    generator.AddTestResult(result1);
    generator.AddTestResult(result2);
    
    // Generate HTML report
    if (generator.GenerateReport("test_report.html")) {
        std::cout << "✓ HTML report generated successfully!" << std::endl;
    } else {
        std::cout << "✗ Failed to generate HTML report" << std::endl;
        assert(false);
    }
}

void test_event_data_batch() {
    std::cout << "Testing EventDataBatch (simplified)..." << std::endl;
    
    // Simple test without protobuf serialization for now
    EventDataBatch batch;
    batch.SetSourceId(1);
    batch.SetSequenceNumber(123);
    
    assert(batch.GetSourceId() == 1);
    assert(batch.GetSequenceNumber() == 123);
    
    std::cout << "  Source ID: " << batch.GetSourceId() << std::endl;
    std::cout << "  Sequence: " << batch.GetSequenceNumber() << std::endl;
    std::cout << "  Events: " << batch.GetEventCount() << std::endl;
    std::cout << "  Data Size: " << batch.GetDataSize() << " bytes" << std::endl;
    
    std::cout << "✓ EventDataBatch test passed!" << std::endl;
}

int main() {
    std::cout << "Running basic functionality tests..." << std::endl;
    std::cout << "======================================" << std::endl;
    
    try {
        test_stats_collector();
        std::cout << std::endl;
        
        test_html_report_generator();
        std::cout << std::endl;
        
        test_event_data_batch();
        std::cout << std::endl;
        
        std::cout << "======================================" << std::endl;
        std::cout << "All tests passed! ✓" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}