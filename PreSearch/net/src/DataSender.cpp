#include "Common.hpp"
#include "Config.hpp"
#include "ITransport.hpp"
#include "EventGenerator.hpp"
#include "Logger.hpp"
#include "StatsCollector.hpp"
#include "MemoryMonitor.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

namespace delila {
namespace test {

class DataSender {
public:
    DataSender(uint32_t sourceId) 
        : sourceId(sourceId), isRunning(false), 
          generator(sourceId), stats(), memoryMonitor() {
        
        logger = Logger::GetLogger(ComponentType::DATA_SENDER);
    }
    
    ~DataSender() {
        Stop();
    }
    
    bool Initialize(const Config& config) {
        this->config = config;
        
        // Initialize logger
        Logger::Initialize(config.GetLoggingConfig().directory);
        
        // Create transport
        transport = TransportFactory::Create(config.GetTestConfig().protocol, 
                                           ComponentType::DATA_SENDER);
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
        
        logger->Info("DataSender initialized with source ID %u", sourceId);
        return true;
    }
    
    void Run(const TestScenario& scenario) {
        if (isRunning.load()) {
            logger->Warning("DataSender already running");
            return;
        }
        
        isRunning.store(true);
        stats.Start();
        
        logger->Info("Starting test scenario: protocol=%s, batchSize=%u, duration=%u minutes",
                    scenario.protocol == TransportType::GRPC ? "gRPC" : "ZeroMQ",
                    scenario.batchSize, scenario.durationMinutes);
        
        uint64_t sequenceNumber = 1;
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::minutes(scenario.durationMinutes);
        
        while (isRunning.load() && std::chrono::steady_clock::now() < endTime) {
            // Check memory usage
            if (memoryMonitor.IsMemoryUsageHigh()) {
                logger->Error("Memory usage exceeded threshold, stopping");
                break;
            }
            
            // Generate batch
            EventDataBatch batch = generator.GenerateBatch(scenario.batchSize, sequenceNumber++);
            
            // Send batch
            auto sendStart = std::chrono::steady_clock::now();
            bool success = transport->Send(batch);
            auto sendEnd = std::chrono::steady_clock::now();
            
            if (success) {
                double latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    sendEnd - sendStart).count();
                stats.RecordMessage(batch.GetDataSize(), latencyUs);
                
                // Record system metrics periodically
                if (sequenceNumber % 100 == 0) {
                    stats.RecordSystemMetrics(memoryMonitor.GetCurrentCpuUsage(),
                                            memoryMonitor.GetCurrentMemoryUsage());
                }
            } else {
                logger->Error("Failed to send batch %lu", sequenceNumber - 1);
                break;
            }
            
            // Maximum throughput mode - no artificial delays
        }
        
        stats.Stop();
        isRunning.store(false);
        
        logger->Info("Test completed. Generated %lu batches", sequenceNumber - 1);
        logger->Info("Final stats: %.2f MB/s, %.2f msgs/s", 
                    stats.GetCurrentThroughputMBps(), 
                    stats.GetCurrentThroughputMsgsPerSec());
    }
    
    void Stop() {
        if (isRunning.load()) {
            isRunning.store(false);
            logger->Info("DataSender stopped");
        }
        
        memoryMonitor.Stop();
        
        if (transport) {
            transport->Shutdown();
        }
    }
    
    StatsReport GetStats() const {
        return stats.GenerateReport();
    }
    
private:
    uint32_t sourceId;
    std::atomic<bool> isRunning;
    
    Config config;
    std::unique_ptr<ITransport> transport;
    EventGenerator generator;
    StatsCollector stats;
    MemoryMonitor memoryMonitor;
    std::shared_ptr<Logger> logger;
};

}} // namespace delila::test

// Global variables for signal handling
std::unique_ptr<delila::test::DataSender> g_sender;
std::atomic<bool> g_shutdown(false);

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_shutdown.store(true);
    if (g_sender) {
        g_sender->Stop();
    }
}

int main(int argc, char* argv[]) {
    using namespace delila::test;
    
    // Parse command line arguments
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <source_id>" << std::endl;
        return 1;
    }
    
    std::string configFile = argv[1];
    uint32_t sourceId = std::stoul(argv[2]);
    
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
        
        // Create and initialize sender
        g_sender = std::make_unique<DataSender>(sourceId);
        if (!g_sender->Initialize(config)) {
            std::cerr << "Failed to initialize DataSender" << std::endl;
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
            g_sender->Run(scenario);
            
            if (g_shutdown.load()) {
                break;
            }
            
            // Brief pause between tests
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        // Get final statistics
        auto finalStats = g_sender->GetStats();
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