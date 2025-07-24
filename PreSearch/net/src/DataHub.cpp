#include "Common.hpp"
#include "Config.hpp"
#include "ITransport.hpp"
#include "EventDataBatch.hpp"
#include "Logger.hpp"
#include "StatsCollector.hpp"
#include "MemoryMonitor.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace delila {
namespace test {

class DataHub {
public:
    DataHub() : isRunning(false), stats(), memoryMonitor() {
        logger = Logger::GetLogger(ComponentType::DATA_HUB);
    }
    
    ~DataHub() {
        Stop();
    }
    
    bool Initialize(const Config& config) {
        this->config = config;
        
        // Initialize logger
        Logger::Initialize(config.GetLoggingConfig().directory);
        
        // Create transport (handles both input and output)
        transport = TransportFactory::Create(config.GetTestConfig().protocol, 
                                           ComponentType::DATA_HUB);
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
        
        logger->Info("DataHub initialized");
        return true;
    }
    
    void Run() {
        if (isRunning.load()) {
            logger->Warning("DataHub already running");
            return;
        }
        
        isRunning.store(true);
        stats.Start();
        
        logger->Info("Starting DataHub");
        
        // Start worker threads
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back(&DataHub::WorkerThread, this);
        }
        
        // Main receiving loop
        while (isRunning.load()) {
            // Check memory usage
            if (memoryMonitor.IsMemoryUsageHigh()) {
                logger->Error("Memory usage exceeded threshold, stopping");
                break;
            }
            
            // Receive batch from senders
            EventDataBatch batch;
            if (transport->Receive(batch)) {
                // Add to processing queue
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    messageQueue.push(batch);
                }
                queueCondition.notify_one();
                
                stats.RecordMessage(batch.GetDataSize());
                
                // Log progress periodically
                static uint64_t messageCount = 0;
                if (++messageCount % 1000 == 0) {
                    logger->Info("Processed %lu messages", messageCount);
                }
            } else {
                // Brief pause if no message
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        // Stop worker threads
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        stats.Stop();
        isRunning.store(false);
        
        logger->Info("DataHub stopped");
    }
    
    void Stop() {
        if (isRunning.load()) {
            isRunning.store(false);
            queueCondition.notify_all();
            logger->Info("DataHub stopping");
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
    std::atomic<bool> isRunning;
    
    Config config;
    std::unique_ptr<ITransport> transport;
    StatsCollector stats;
    MemoryMonitor memoryMonitor;
    std::shared_ptr<Logger> logger;
    
    // Message queue for worker threads
    std::queue<EventDataBatch> messageQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    
    void WorkerThread() {
        logger->Info("Worker thread started");
        
        while (isRunning.load()) {
            EventDataBatch batch;
            bool hasMessage = false;
            
            // Get message from queue
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCondition.wait(lock, [this] { 
                    return !messageQueue.empty() || !isRunning.load(); 
                });
                
                if (!messageQueue.empty()) {
                    batch = messageQueue.front();
                    messageQueue.pop();
                    hasMessage = true;
                }
            }
            
            if (hasMessage) {
                // Process message (for now, just forward to output)
                // In a real implementation, this would forward to receiver sinks
                ProcessMessage(batch);
            }
        }
        
        logger->Info("Worker thread stopped");
    }
    
    void ProcessMessage(const EventDataBatch& batch) {
        // Forward message to receivers via transport
        if (transport && transport->Send(batch)) {
            // Message forwarded successfully
        } else {
            logger->Error("Failed to forward message to receivers");
        }
    }
};

}} // namespace delila::test

// Global variables for signal handling
std::unique_ptr<delila::test::DataHub> g_hub;
std::atomic<bool> g_shutdown(false);

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_shutdown.store(true);
    if (g_hub) {
        g_hub->Stop();
    }
}

int main(int argc, char* argv[]) {
    using namespace delila::test;
    
    // Parse command line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    std::string configFile = argv[1];
    
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
        
        // Create and initialize hub
        g_hub = std::make_unique<DataHub>();
        if (!g_hub->Initialize(config)) {
            std::cerr << "Failed to initialize DataHub" << std::endl;
            return 1;
        }
        
        std::cout << "DataHub starting..." << std::endl;
        g_hub->Run();
        
        // Get final statistics
        auto finalStats = g_hub->GetStats();
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