#include "Common.hpp"
#include "Config.hpp"
#include "ITransport.hpp"
#include "EventDataBatch.hpp"
#include "Logger.hpp"
#include "StatsCollector.hpp"
#include "MemoryMonitor.hpp"
#include "SequenceValidator.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <fstream>

namespace delila {
namespace test {

class DataReceiver {
public:
    DataReceiver(uint32_t sinkId) 
        : sinkId(sinkId), isRunning(false), 
          stats(), memoryMonitor(), validator() {
        
        logger = Logger::GetLogger(ComponentType::DATA_RECEIVER);
    }
    
    ~DataReceiver() {
        Stop();
    }
    
    bool Initialize(const Config& config) {
        this->config = config;
        
        // Initialize logger
        Logger::Initialize(config.GetLoggingConfig().directory);
        
        // Create transport
        transport = TransportFactory::Create(config.GetTestConfig().protocol, 
                                           ComponentType::DATA_RECEIVER);
        if (!transport) {
            logger->Error("Failed to create transport");
            return false;
        }
        
        // Initialize transport
        if (!transport->Initialize(config)) {
            logger->Error("Failed to initialize transport");
            return false;
        }
        
        // Start memory monitoring
        memoryMonitor.Start();
        
        logger->Info("DataReceiver initialized with sink ID %u", sinkId);
        return true;
    }
    
    void Run(const TestScenario& scenario) {
        if (isRunning.load()) {
            logger->Warning("DataReceiver already running");
            return;
        }
        
        isRunning.store(true);
        stats.Start();
        validator.Reset();
        
        logger->Info("Starting test scenario: protocol=%s, batchSize=%u, duration=%u minutes",
                    scenario.protocol == TransportType::GRPC ? "gRPC" : "ZeroMQ",
                    scenario.batchSize, scenario.durationMinutes);
        
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::minutes(scenario.durationMinutes);
        
        while (isRunning.load() && std::chrono::steady_clock::now() < endTime) {
            // Check memory usage
            if (memoryMonitor.IsMemoryUsageHigh()) {
                logger->Error("Memory usage exceeded threshold, stopping");
                break;
            }
            
            // Receive batch
            EventDataBatch batch;
            auto receiveStart = std::chrono::steady_clock::now();
            bool success = transport->Receive(batch);
            auto receiveEnd = std::chrono::steady_clock::now();
            
            if (success) {
                double latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    receiveEnd - receiveStart).count();
                stats.RecordMessage(batch.GetDataSize(), latencyUs);
                
                // Validate sequence
                validator.CheckSequence(batch.GetSequenceNumber(), batch.GetSourceId());
                
                // Record system metrics periodically
                static uint64_t messageCount = 0;
                if (++messageCount % 100 == 0) {
                    stats.RecordSystemMetrics(memoryMonitor.GetCurrentCpuUsage(),
                                            memoryMonitor.GetCurrentMemoryUsage());
                }
                
                // Log progress periodically
                if (messageCount % 1000 == 0) {
                    logger->Info("Received %lu messages, %.2f MB/s", 
                                messageCount, stats.GetCurrentThroughputMBps());
                }
            } else {
                // Non-blocking receive might return false, brief pause
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        stats.Stop();
        isRunning.store(false);
        
        // Generate final report
        GenerateReport(scenario);
        
        logger->Info("Test completed");
    }
    
    void Stop() {
        if (isRunning.load()) {
            isRunning.store(false);
            logger->Info("DataReceiver stopped");
        }
        
        memoryMonitor.Stop();
        
        if (transport) {
            transport->Shutdown();
        }
    }
    
    StatsReport GetStats() const {
        return stats.GenerateReport();
    }
    
    void GenerateReport(const TestScenario& scenario) {
        auto statsReport = stats.GenerateReport();
        auto validationStats = validator.GetStats();
        
        // Create output filename
        std::string filename = "receiver_" + std::to_string(sinkId) + "_" + 
                              TransportTypeToString(scenario.protocol) + "_" +
                              std::to_string(scenario.batchSize) + "_events.json";
        
        std::string outputPath = config.GetResultsFilePath(filename);
        
        // Write JSON report
        std::ofstream file(outputPath);
        if (file.is_open()) {
            file << "{\n";
            file << "  \"test_info\": {\n";
            file << "    \"protocol\": \"" << TransportTypeToString(scenario.protocol) << "\",\n";
            file << "    \"batch_size\": " << scenario.batchSize << ",\n";
            file << "    \"sink_id\": " << sinkId << ",\n";
            file << "    \"duration_minutes\": " << scenario.durationMinutes << "\n";
            file << "  },\n";
            
            file << "  \"performance\": {\n";
            file << "    \"messages_received\": " << statsReport.messagesReceived << ",\n";
            file << "    \"bytes_received\": " << statsReport.bytesReceived << ",\n";
            file << "    \"throughput_mbps\": " << statsReport.throughputMBps << ",\n";
            file << "    \"throughput_msgs_per_sec\": " << statsReport.throughputMsgsPerSec << ",\n";
            file << "    \"latency_mean_us\": " << statsReport.latencyMean << ",\n";
            file << "    \"latency_min_us\": " << statsReport.latencyMin << ",\n";
            file << "    \"latency_max_us\": " << statsReport.latencyMax << ",\n";
            file << "    \"latency_50th_us\": " << statsReport.latency50th << ",\n";
            file << "    \"latency_90th_us\": " << statsReport.latency90th << ",\n";
            file << "    \"latency_99th_us\": " << statsReport.latency99th << ",\n";
            file << "    \"cpu_usage\": " << statsReport.cpuUsage << ",\n";
            file << "    \"memory_usage\": " << statsReport.memoryUsage << "\n";
            file << "  },\n";
            
            file << "  \"validation\": {\n";
            file << "    \"total_sequences\": " << validationStats.totalSequences << ",\n";
            file << "    \"duplicate_sequences\": " << validationStats.duplicateSequences << ",\n";
            file << "    \"out_of_order_sequences\": " << validationStats.outOfOrderSequences << ",\n";
            file << "    \"missing_sequences\": " << validationStats.missingSequences << ",\n";
            file << "    \"last_received_sequence\": " << validationStats.lastReceivedSequence << "\n";
            file << "  }\n";
            file << "}\n";
            
            file.close();
            logger->Info("Report written to %s", outputPath.c_str());
        } else {
            logger->Error("Failed to write report to %s", outputPath.c_str());
        }
    }
    
private:
    uint32_t sinkId;
    std::atomic<bool> isRunning;
    
    Config config;
    std::unique_ptr<ITransport> transport;
    StatsCollector stats;
    MemoryMonitor memoryMonitor;
    SequenceValidator validator;
    std::shared_ptr<Logger> logger;
};

}} // namespace delila::test

// Global variables for signal handling
std::unique_ptr<delila::test::DataReceiver> g_receiver;
std::atomic<bool> g_shutdown(false);

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_shutdown.store(true);
    if (g_receiver) {
        g_receiver->Stop();
    }
}

int main(int argc, char* argv[]) {
    using namespace delila::test;
    
    // Parse command line arguments
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <sink_id>" << std::endl;
        return 1;
    }
    
    std::string configFile = argv[1];
    uint32_t sinkId = std::stoul(argv[2]);
    
    // Install signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    try {
        // Load configuration
        Config config;
        if (!config.LoadFromFile(configFile)) {
            std::cerr << "Failed to load configuration from " << configFile << std::endl;
            return 1;
        }
        
        // Create and initialize receiver
        g_receiver = std::make_unique<DataReceiver>(sinkId);
        if (!g_receiver->Initialize(config)) {
            std::cerr << "Failed to initialize DataReceiver" << std::endl;
            return 1;
        }
        
        // Run for each batch size
        for (uint32_t batchSize : config.GetTestConfig().batchSizes) {
            if (g_shutdown.load()) {
                break;
            }
            
            TestScenario scenario;
            scenario.protocol = config.GetTestConfig().protocol;
            scenario.transport = config.GetTestConfig().transport;
            scenario.batchSize = batchSize;
            scenario.durationMinutes = config.GetTestConfig().durationMinutes;
            scenario.outputDir = config.GetTestConfig().outputDir;
            
            std::cout << "Running test with batch size " << batchSize << " events" << std::endl;
            g_receiver->Run(scenario);
            
            if (g_shutdown.load()) {
                break;
            }
            
            // Brief pause between tests
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        // Get final statistics
        auto finalStats = g_receiver->GetStats();
        std::cout << "Final statistics:" << std::endl;
        std::cout << "  Total messages: " << finalStats.messagesReceived << std::endl;
        std::cout << "  Total bytes: " << finalStats.bytesReceived << std::endl;
        std::cout << "  Average throughput: " << finalStats.throughputMBps << " MB/s" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}