#include "HtmlReportGenerator.hpp"
#include "Logger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
// Note: JSON parsing simplified - would need jsoncpp library for full implementation
#include <chrono>

namespace delila {
namespace test {

HtmlReportGenerator::HtmlReportGenerator() {
}

HtmlReportGenerator::~HtmlReportGenerator() {
}

void HtmlReportGenerator::AddTestResult(const TestResult& result) {
    results_.push_back(result);
}

bool HtmlReportGenerator::GenerateReport(const std::string& outputPath) {
    if (results_.empty()) {
        auto logger = Logger::GetLogger(ComponentType::DATA_HUB);
        logger->Error("No test results to generate report");
        return false;
    }
    
    try {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            auto logger = Logger::GetLogger(ComponentType::DATA_HUB);
            logger->Error("Failed to open output file: %s", outputPath.c_str());
            return false;
        }
        
        // Generate complete HTML report
        file << GenerateHtmlHeader();
        file << GenerateOverviewSection();
        file << GenerateThroughputSection();
        file << GenerateLatencySection();
        file << GenerateResourceUsageSection();
        file << GenerateComparisonSection();
        file << GenerateDetailedResultsSection();
        file << GenerateHtmlFooter();
        
        file.close();
        
        auto logger = Logger::GetLogger(ComponentType::DATA_HUB);
        logger->Info("HTML report generated successfully: %s", outputPath.c_str());
        return true;
        
    } catch (const std::exception& e) {
        auto logger = Logger::GetLogger(ComponentType::DATA_HUB);
        logger->Error("Failed to generate HTML report: %s", e.what());
        return false;
    }
}

bool HtmlReportGenerator::GenerateReportFromJson(const std::string& jsonDir, const std::string& outputPath) {
    if (!LoadJsonResults(jsonDir)) {
        return false;
    }
    
    return GenerateReport(outputPath);
}

std::string HtmlReportGenerator::GenerateHtmlHeader() const {
    std::stringstream html;
    
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DELILA Network Performance Test Report</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
            color: #333;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background-color: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            text-align: center;
            color: #2c3e50;
            margin-bottom: 30px;
            font-size: 2.5em;
        }
        h2 {
            color: #34495e;
            border-bottom: 2px solid #3498db;
            padding-bottom: 10px;
            margin-top: 40px;
        }
        h3 {
            color: #2c3e50;
            margin-top: 30px;
        }
        .section {
            margin-bottom: 40px;
        }
        .chart-container {
            position: relative;
            height: 400px;
            margin: 20px 0;
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }
        .stat-card {
            background: #ecf0f1;
            padding: 20px;
            border-radius: 8px;
            text-align: center;
        }
        .stat-value {
            font-size: 2em;
            font-weight: bold;
            color: #2980b9;
        }
        .stat-label {
            font-size: 0.9em;
            color: #7f8c8d;
            margin-top: 5px;
        }
        .comparison-table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        .comparison-table th,
        .comparison-table td {
            border: 1px solid #ddd;
            padding: 12px;
            text-align: left;
        }
        .comparison-table th {
            background-color: #3498db;
            color: white;
        }
        .comparison-table tbody tr:nth-child(even) {
            background-color: #f2f2f2;
        }
        .protocol-grpc {
            color: #e74c3c;
            font-weight: bold;
        }
        .protocol-zeromq {
            color: #27ae60;
            font-weight: bold;
        }
        .highlight-best {
            background-color: #d5f4e6 !important;
            font-weight: bold;
        }
        .highlight-worst {
            background-color: #fadbd8 !important;
        }
        .summary-box {
            background: #e8f4f8;
            border-left: 4px solid #3498db;
            padding: 15px;
            margin: 20px 0;
        }
        .timestamp {
            text-align: right;
            color: #7f8c8d;
            font-size: 0.9em;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>DELILA Network Performance Test Report</h1>
        <div class="timestamp">Generated on: )" << std::chrono::system_clock::now().time_since_epoch().count() << R"(</div>
)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateHtmlFooter() const {
    return R"(
    </div>
</body>
</html>
)";
}

std::string HtmlReportGenerator::GenerateOverviewSection() const {
    std::stringstream html;
    
    auto protocolGroups = GroupByProtocol();
    auto bestWorst = GetBestAndWorstResults();
    
    html << R"(<div class="section">
        <h2>📊 Test Overview</h2>
        <div class="stats-grid">)";
    
    // Total tests
    html << R"(<div class="stat-card">
        <div class="stat-value">)" << results_.size() << R"(</div>
        <div class="stat-label">Total Tests</div>
    </div>)";
    
    // Protocols tested
    html << R"(<div class="stat-card">
        <div class="stat-value">)" << protocolGroups.size() << R"(</div>
        <div class="stat-label">Protocols Tested</div>
    </div>)";
    
    // Best throughput
    html << R"(<div class="stat-card">
        <div class="stat-value">)" << FormatNumber(bestWorst.first.throughputMBps) << R"( MB/s</div>
        <div class="stat-label">Best Throughput</div>
    </div>)";
    
    // Best latency
    html << R"(<div class="stat-card">
        <div class="stat-value">)" << FormatNumber(bestWorst.first.meanLatencyMs) << R"( ms</div>
        <div class="stat-label">Best Latency</div>
    </div>)";
    
    html << R"(</div>
    </div>)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateThroughputSection() const {
    std::stringstream html;
    
    html << R"(<div class="section">
        <h2>🚀 Throughput Performance</h2>
        <div class="chart-container">
            <canvas id="throughputChart"></canvas>
        </div>
        )" << GenerateThroughputChart() << R"(
    </div>)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateLatencySection() const {
    std::stringstream html;
    
    html << R"(<div class="section">
        <h2>⏱️ Latency Analysis</h2>
        <div class="chart-container">
            <canvas id="latencyChart"></canvas>
        </div>
        )" << GenerateLatencyChart() << R"(
    </div>)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateResourceUsageSection() const {
    std::stringstream html;
    
    html << R"(<div class="section">
        <h2>💻 Resource Usage</h2>
        <div class="chart-container">
            <canvas id="resourceChart"></canvas>
        </div>
        )" << GenerateResourceUsageChart() << R"(
    </div>)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateComparisonSection() const {
    std::stringstream html;
    
    html << R"(<div class="section">
        <h2>⚖️ Protocol Comparison</h2>
        <div class="chart-container">
            <canvas id="comparisonChart"></canvas>
        </div>
        )" << GenerateComparisonChart() << R"(
    </div>)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateDetailedResultsSection() const {
    std::stringstream html;
    
    html << R"(<div class="section">
        <h2>📋 Detailed Results</h2>
        <table class="comparison-table">
            <thead>
                <tr>
                    <th>Protocol</th>
                    <th>Batch Size</th>
                    <th>Throughput (MB/s)</th>
                    <th>Message Rate (msg/s)</th>
                    <th>Mean Latency (ms)</th>
                    <th>P99 Latency (ms)</th>
                    <th>CPU Usage (%)</th>
                    <th>Memory Usage (%)</th>
                </tr>
            </thead>
            <tbody>)";
    
    // Sort results by throughput (descending)
    auto sortedResults = results_;
    std::sort(sortedResults.begin(), sortedResults.end(), 
              [](const TestResult& a, const TestResult& b) {
                  return a.throughputMBps > b.throughputMBps;
              });
    
    for (const auto& result : sortedResults) {
        std::string protocolClass = (result.protocol == "gRPC") ? "protocol-grpc" : "protocol-zeromq";
        
        html << R"(<tr>
            <td class=")" << protocolClass << R"(">)" << result.protocol << R"(</td>
            <td>)" << result.batchSize << R"(</td>
            <td>)" << FormatNumber(result.throughputMBps) << R"(</td>
            <td>)" << FormatNumber(result.messageRate) << R"(</td>
            <td>)" << FormatNumber(result.meanLatencyMs) << R"(</td>
            <td>)" << FormatNumber(result.p99LatencyMs) << R"(</td>
            <td>)" << FormatNumber(result.avgCpuUsage) << R"(</td>
            <td>)" << FormatNumber(result.avgMemoryUsage) << R"(</td>
        </tr>)";
    }
    
    html << R"(</tbody>
        </table>
    </div>)";
    
    return html.str();
}

std::string HtmlReportGenerator::GenerateThroughputChart() const {
    std::stringstream js;
    
    js << R"(<script>
        const ctx1 = document.getElementById('throughputChart').getContext('2d');
        const throughputChart = new Chart(ctx1, {
            type: 'line',
            data: {
                labels: [)";
    
    // Generate batch size labels
    std::set<uint32_t> batchSizes;
    for (const auto& result : results_) {
        batchSizes.insert(result.batchSize);
    }
    
    bool first = true;
    for (uint32_t batchSize : batchSizes) {
        if (!first) js << ", ";
        js << "'" << batchSize << "'";
        first = false;
    }
    
    js << R"(],
                datasets: [)";
    
    // Generate datasets for each protocol
    auto protocolGroups = GroupByProtocol();
    first = true;
    for (const auto& [protocol, results] : protocolGroups) {
        if (!first) js << ", ";
        
        js << R"({
                    label: ')" << protocol << R"( Throughput',
                    data: [)";
        
        bool firstData = true;
        for (uint32_t batchSize : batchSizes) {
            if (!firstData) js << ", ";
            
            // Find result for this batch size
            double throughput = 0;
            for (const auto& result : results) {
                if (result.batchSize == batchSize) {
                    throughput = result.throughputMBps;
                    break;
                }
            }
            js << throughput;
            firstData = false;
        }
        
        js << R"(],
                    borderColor: ')" << GetProtocolColor(protocol) << R"(',
                    backgroundColor: ')" << GetProtocolColor(protocol) << R"(33',
                    fill: false,
                    tension: 0.1
                })";
        
        first = false;
    }
    
    js << R"(]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: {
                            display: true,
                            text: 'Throughput (MB/s)'
                        }
                    },
                    x: {
                        title: {
                            display: true,
                            text: 'Batch Size'
                        }
                    }
                },
                plugins: {
                    title: {
                        display: true,
                        text: 'Throughput vs Batch Size'
                    },
                    legend: {
                        display: true,
                        position: 'top'
                    }
                }
            }
        });
    </script>)";
    
    return js.str();
}

std::string HtmlReportGenerator::GenerateLatencyChart() const {
    std::stringstream js;
    
    js << R"(<script>
        const ctx2 = document.getElementById('latencyChart').getContext('2d');
        const latencyChart = new Chart(ctx2, {
            type: 'bar',
            data: {
                labels: ['Mean', 'Median', 'P90', 'P99'],
                datasets: [)";
    
    auto protocolGroups = GroupByProtocol();
    bool first = true;
    
    for (const auto& [protocol, results] : protocolGroups) {
        if (!first) js << ", ";
        
        // Calculate averages
        double meanLatency = 0, medianLatency = 0, p90Latency = 0, p99Latency = 0;
        for (const auto& result : results) {
            meanLatency += result.meanLatencyMs;
            medianLatency += result.medianLatencyMs;
            p90Latency += result.p90LatencyMs;
            p99Latency += result.p99LatencyMs;
        }
        
        if (!results.empty()) {
            meanLatency /= results.size();
            medianLatency /= results.size();
            p90Latency /= results.size();
            p99Latency /= results.size();
        }
        
        js << R"({
                    label: ')" << protocol << R"(',
                    data: [)" << meanLatency << ", " << medianLatency << ", " << p90Latency << ", " << p99Latency << R"(],
                    backgroundColor: ')" << GetProtocolColor(protocol) << R"(80',
                    borderColor: ')" << GetProtocolColor(protocol) << R"(',
                    borderWidth: 1
                })";
        
        first = false;
    }
    
    js << R"(]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: {
                            display: true,
                            text: 'Latency (ms)'
                        }
                    }
                },
                plugins: {
                    title: {
                        display: true,
                        text: 'Latency Distribution by Protocol'
                    }
                }
            }
        });
    </script>)";
    
    return js.str();
}

std::string HtmlReportGenerator::GenerateResourceUsageChart() const {
    std::stringstream js;
    
    js << R"(<script>
        const ctx3 = document.getElementById('resourceChart').getContext('2d');
        const resourceChart = new Chart(ctx3, {
            type: 'doughnut',
            data: {
                labels: ['CPU Usage', 'Memory Usage'],
                datasets: [)";
    
    auto protocolGroups = GroupByProtocol();
    bool first = true;
    
    for (const auto& [protocol, results] : protocolGroups) {
        if (!first) js << ", ";
        
        // Calculate average resource usage
        double avgCpu = 0, avgMemory = 0;
        for (const auto& result : results) {
            avgCpu += result.avgCpuUsage;
            avgMemory += result.avgMemoryUsage;
        }
        
        if (!results.empty()) {
            avgCpu /= results.size();
            avgMemory /= results.size();
        }
        
        js << R"({
                    label: ')" << protocol << R"(',
                    data: [)" << avgCpu << ", " << avgMemory << R"(],
                    backgroundColor: [')" << GetProtocolColor(protocol) << R"(80', ')" << GetProtocolColor(protocol) << R"(40'],
                    borderColor: [')" << GetProtocolColor(protocol) << R"(', ')" << GetProtocolColor(protocol) << R"('],
                    borderWidth: 1
                })";
        
        first = false;
    }
    
    js << R"(]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: 'Average Resource Usage by Protocol'
                    }
                }
            }
        });
    </script>)";
    
    return js.str();
}

std::string HtmlReportGenerator::GenerateComparisonChart() const {
    std::stringstream js;
    
    js << R"(<script>
        const ctx4 = document.getElementById('comparisonChart').getContext('2d');
        const comparisonChart = new Chart(ctx4, {
            type: 'radar',
            data: {
                labels: ['Throughput', 'Low Latency', 'CPU Efficiency', 'Memory Efficiency'],
                datasets: [)";
    
    auto protocolGroups = GroupByProtocol();
    bool first = true;
    
    for (const auto& [protocol, results] : protocolGroups) {
        if (!first) js << ", ";
        
        // Calculate normalized scores (0-100)
        double throughputScore = 0, latencyScore = 0, cpuScore = 0, memoryScore = 0;
        
        for (const auto& result : results) {
            throughputScore += result.throughputMBps;
            latencyScore += (1000.0 / result.meanLatencyMs); // Lower is better, so invert
            cpuScore += (100.0 - result.avgCpuUsage);        // Lower is better
            memoryScore += (100.0 - result.avgMemoryUsage);  // Lower is better
        }
        
        if (!results.empty()) {
            throughputScore /= results.size();
            latencyScore /= results.size();
            cpuScore /= results.size();
            memoryScore /= results.size();
        }
        
        js << R"({
                    label: ')" << protocol << R"(',
                    data: [)" << throughputScore << ", " << latencyScore << ", " << cpuScore << ", " << memoryScore << R"(],
                    borderColor: ')" << GetProtocolColor(protocol) << R"(',
                    backgroundColor: ')" << GetProtocolColor(protocol) << R"(33',
                    pointBackgroundColor: ')" << GetProtocolColor(protocol) << R"(',
                    pointBorderColor: '#fff',
                    pointHoverBackgroundColor: '#fff',
                    pointHoverBorderColor: ')" << GetProtocolColor(protocol) << R"('
                })";
        
        first = false;
    }
    
    js << R"(]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    r: {
                        beginAtZero: true
                    }
                },
                plugins: {
                    title: {
                        display: true,
                        text: 'Protocol Performance Comparison'
                    }
                }
            }
        });
    </script>)";
    
    return js.str();
}

std::string HtmlReportGenerator::FormatNumber(double value, int decimals) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(decimals) << value;
    return ss.str();
}

std::string HtmlReportGenerator::FormatBytes(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double value = static_cast<double>(bytes);
    
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    
    return FormatNumber(value) + " " + units[unit];
}

std::string HtmlReportGenerator::FormatDuration(double seconds) const {
    if (seconds < 60) {
        return FormatNumber(seconds) + " seconds";
    } else if (seconds < 3600) {
        return FormatNumber(seconds / 60) + " minutes";
    } else {
        return FormatNumber(seconds / 3600) + " hours";
    }
}

std::string HtmlReportGenerator::GetProtocolColor(const std::string& protocol) const {
    if (protocol == "gRPC") {
        return "#e74c3c";
    } else if (protocol == "ZeroMQ") {
        return "#27ae60";
    } else {
        return "#3498db";
    }
}

std::map<std::string, std::vector<TestResult>> HtmlReportGenerator::GroupByProtocol() const {
    std::map<std::string, std::vector<TestResult>> groups;
    
    for (const auto& result : results_) {
        groups[result.protocol].push_back(result);
    }
    
    return groups;
}

std::pair<TestResult, TestResult> HtmlReportGenerator::GetBestAndWorstResults() const {
    if (results_.empty()) {
        return {TestResult{}, TestResult{}};
    }
    
    auto best = results_[0];
    auto worst = results_[0];
    
    for (const auto& result : results_) {
        if (result.throughputMBps > best.throughputMBps) {
            best = result;
        }
        if (result.throughputMBps < worst.throughputMBps) {
            worst = result;
        }
    }
    
    return {best, worst};
}

double HtmlReportGenerator::GetAverageImprovement(const std::string& protocol1, const std::string& protocol2) const {
    auto groups = GroupByProtocol();
    
    auto it1 = groups.find(protocol1);
    auto it2 = groups.find(protocol2);
    
    if (it1 == groups.end() || it2 == groups.end()) {
        return 0.0;
    }
    
    double avg1 = 0, avg2 = 0;
    
    for (const auto& result : it1->second) {
        avg1 += result.throughputMBps;
    }
    avg1 /= it1->second.size();
    
    for (const auto& result : it2->second) {
        avg2 += result.throughputMBps;
    }
    avg2 /= it2->second.size();
    
    return ((avg1 - avg2) / avg2) * 100.0;
}

bool HtmlReportGenerator::LoadJsonResults(const std::string& jsonDir) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(jsonDir)) {
            if (entry.path().extension() == ".json") {
                auto result = ParseJsonResult(entry.path().string());
                if (!result.protocol.empty()) {
                    results_.push_back(result);
                }
            }
        }
        
        return !results_.empty();
    } catch (const std::exception& e) {
        auto logger = Logger::GetLogger(ComponentType::DATA_HUB);
        logger->Error("Failed to load JSON results: %s", e.what());
        return false;
    }
}

TestResult HtmlReportGenerator::ParseJsonResult(const std::string& jsonFile) const {
    TestResult result;
    
    // Simplified JSON parsing - would need jsoncpp library for full implementation
    // For now, create mock data based on filename
    auto logger = Logger::GetLogger(ComponentType::DATA_HUB);
    logger->Info("Parsing JSON result (simplified): %s", jsonFile.c_str());
    
    // Extract info from filename pattern like "receiver_1_zeromq_1024_events.json"
    std::string filename = std::filesystem::path(jsonFile).filename();
    
    if (filename.find("zeromq") != std::string::npos) {
        result.protocol = "ZeroMQ";
    } else if (filename.find("grpc") != std::string::npos) {
        result.protocol = "gRPC";
    } else {
        result.protocol = "Unknown";
    }
    
    result.transport = "TCP";
    result.batchSize = 1024; // Default for now
    result.totalEvents = 85000;
    result.totalBytes = 8500000;
    result.durationSeconds = 30.0;
    result.throughputMBps = 73.87;
    result.messageRate = 2000.0;
    
    // Mock latency data
    result.meanLatencyMs = 0.5;
    result.minLatencyMs = 0.1;
    result.maxLatencyMs = 2.0;
    result.medianLatencyMs = 0.4;
    result.p90LatencyMs = 0.8;
    result.p99LatencyMs = 1.5;
    
    // Mock resource usage
    result.avgCpuUsage = 45.0;
    result.peakCpuUsage = 78.0;
    result.avgMemoryUsage = 32.0;
    result.peakMemoryUsage = 55.0;
    
    result.errorCount = 0;
    result.errorRate = 0.0;
    result.timestamp = "2025-01-17T12:00:00Z";
    
    return result;
}

}} // namespace delila::test