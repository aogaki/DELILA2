#include <iostream>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <thread>
#include <signal.h>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

// Global flag for graceful shutdown
volatile bool running = true;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully...\n";
    running = false;
}

class PerformanceMonitor {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point last_update;
    size_t total_events_received = 0;
    size_t total_bytes_received = 0;
    size_t events_in_interval = 0;
    size_t bytes_in_interval = 0;
    std::chrono::seconds update_interval;
    
public:
    PerformanceMonitor(std::chrono::seconds interval = std::chrono::seconds(5)) 
        : update_interval(interval) {
        Reset();
    }
    
    void Reset() {
        start_time = std::chrono::high_resolution_clock::now();
        last_update = start_time;
        total_events_received = 0;
        total_bytes_received = 0;
        events_in_interval = 0;
        bytes_in_interval = 0;
    }
    
    void RecordEvents(size_t count, size_t bytes) {
        total_events_received += count;
        total_bytes_received += bytes;
        events_in_interval += count;
        bytes_in_interval += bytes;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto since_update = std::chrono::duration_cast<std::chrono::seconds>(now - last_update);
        
        if (since_update >= update_interval) {
            PrintIntervalStats(since_update);
            events_in_interval = 0;
            bytes_in_interval = 0;
            last_update = now;
        }
    }
    
    void PrintIntervalStats(std::chrono::seconds interval) {
        double rate_events = static_cast<double>(events_in_interval) / interval.count();
        double rate_mbps = (bytes_in_interval * 8.0) / (interval.count() * 1e6);
        
        std::cout << "[" << std::setfill('0') << std::setw(8) << total_events_received << "] "
                 << "Rate: " << std::setfill(' ') << std::setw(8) << std::fixed << std::setprecision(1) 
                 << rate_events << " evt/s, " 
                 << std::setw(8) << rate_mbps << " Mbps\n";
    }
    
    void PrintFinalStats() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
            end_time - start_time);
        
        double avg_rate_events = total_events_received / total_elapsed.count();
        double avg_rate_mbps = (total_bytes_received * 8.0) / (total_elapsed.count() * 1e6);
        double avg_bytes_per_event = static_cast<double>(total_bytes_received) / total_events_received;
        
        std::cout << "\n=== Final Performance Statistics ===\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Total events received: " << total_events_received << "\n";
        std::cout << "Total bytes received: " << total_bytes_received 
                 << " (" << total_bytes_received/1024.0/1024.0 << " MB)\n";
        std::cout << "Total elapsed time: " << total_elapsed.count() << " seconds\n";
        std::cout << "Average event rate: " << avg_rate_events << " events/second\n";
        std::cout << "Average data rate: " << avg_rate_mbps << " Mbps\n";
        std::cout << "Average bytes per event: " << avg_bytes_per_event << " bytes\n";
        std::cout << "===================================\n";
    }
    
    size_t GetTotalEvents() const { return total_events_received; }
    size_t GetTotalBytes() const { return total_bytes_received; }
};

void AnalyzeEvents(const std::vector<std::unique_ptr<EventData>>& events) {
    if (events.empty()) return;
    
    // Analyze first few events for demonstration
    static size_t analysis_count = 0;
    if (++analysis_count <= 3) {
        std::cout << "\n--- Event Analysis (Batch " << analysis_count << ") ---\n";
        
        for (size_t i = 0; i < std::min(events.size(), size_t(2)); ++i) {
            const auto& event = events[i];
            std::cout << "Event " << i << ":\n";
            std::cout << "  Timestamp: " << event->timeStampNs << " ns\n";
            std::cout << "  Energy: " << event->energy << " (short: " << event->energyShort << ")\n";
            std::cout << "  Module/Channel: " << event->module << "/" << event->channel << "\n";
            std::cout << "  Waveform size: " << event->waveformSize << " samples\n";
            std::cout << "  Analog probe 1 samples: " << event->analogProbe1.size() << "\n";
            std::cout << "  Digital probe 1 samples: " << event->digitalProbe1.size() << "\n";
            
            // Show first few waveform samples
            if (!event->analogProbe1.empty()) {
                std::cout << "  First 5 analog samples: ";
                for (size_t j = 0; j < std::min(event->analogProbe1.size(), size_t(5)); ++j) {
                    std::cout << event->analogProbe1[j] << " ";
                }
                std::cout << "\n";
            }
            
            // Check flags
            if (event->flags & EventData::FLAG_PILEUP) {
                std::cout << "  [PILEUP DETECTED]\n";
            }
            if (event->flags & EventData::FLAG_OVER_RANGE) {
                std::cout << "  [OVER RANGE DETECTED]\n";
            }
            
            std::cout << "\n";
        }
        std::cout << "--- End Analysis ---\n\n";
    }
}

size_t EstimateEventSize(const EventData& event) {
    return sizeof(EventData) + 
           event.analogProbe1.size() * sizeof(int32_t) +
           event.analogProbe2.size() * sizeof(int32_t) +
           event.digitalProbe1.size() * sizeof(uint8_t) +
           event.digitalProbe2.size() * sizeof(uint8_t) +
           event.digitalProbe3.size() * sizeof(uint8_t) +
           event.digitalProbe4.size() * sizeof(uint8_t);
}

int main(int argc, char* argv[]) {
    // Setup signal handling
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // Parse command line arguments
    std::string address = "tcp://localhost:5555";
    std::chrono::seconds timeout(10);
    bool save_to_file = false;
    std::string output_file = "received_data.log";
    
    if (argc > 1) address = argv[1];
    if (argc > 2) timeout = std::chrono::seconds(std::stoul(argv[2]));
    if (argc > 3) {
        save_to_file = true;
        output_file = argv[3];
    }
    
    std::cout << "DELILA2 Network Library - Data Receiver\n";
    std::cout << "=======================================\n";
    std::cout << "Address: " << address << "\n";
    std::cout << "Timeout: " << timeout.count() << " seconds per receive attempt\n";
    if (save_to_file) {
        std::cout << "Output file: " << output_file << "\n";
    }
    std::cout << "Press Ctrl+C to stop gracefully\n\n";
    
    // Configure transport as subscriber
    TransportConfig config;
    config.data_address = address;
    config.is_publisher = false;
    config.bind_data = false;
    
    // Create and configure transport
    auto transport = std::make_unique<ZMQTransport>();
    if (!transport->Configure(config)) {
        std::cerr << "Failed to configure transport\n";
        return 1;
    }
    
    if (!transport->Connect()) {
        std::cerr << "Failed to connect transport\n";
        return 1;
    }
    
    std::cout << "Subscriber connected. Waiting for data...\n\n";
    
    // Setup performance monitoring
    PerformanceMonitor monitor;
    
    // Optional file output
    std::ofstream log_file;
    if (save_to_file) {
        log_file.open(output_file);
        if (!log_file.is_open()) {
            std::cerr << "Warning: Could not open output file " << output_file << "\n";
            save_to_file = false;
        } else {
            log_file << "timestamp_ns,energy,energy_short,module,channel,waveform_size,flags\n";
        }
    }
    
    size_t consecutive_timeouts = 0;
    const size_t max_timeouts = 3;
    
    std::cout << "Listening for events (showing rate every 5 seconds)...\n";
    
    // Main receive loop
    while (running) {
        auto [events, sequence] = transport->Receive();
        
        if (events && !events->empty()) {
            // Reset timeout counter
            consecutive_timeouts = 0;
            
            // Calculate batch size
            size_t batch_bytes = 0;
            for (const auto& event : *events) {
                batch_bytes += EstimateEventSize(*event);
            }
            
            // Record performance
            monitor.RecordEvents(events->size(), batch_bytes);
            
            // Analyze events (first few batches only)
            AnalyzeEvents(*events);
            
            // Optional: Save to file
            if (save_to_file && log_file.is_open()) {
                for (const auto& event : *events) {
                    log_file << event->timeStampNs << ","
                            << event->energy << ","
                            << event->energyShort << ","
                            << event->module << ","
                            << event->channel << ","
                            << event->waveformSize << ","
                            << event->flags << "\n";
                }
                log_file.flush();
            }
            
        } else {
            // No data received (timeout or error)
            consecutive_timeouts++;
            
            if (consecutive_timeouts >= max_timeouts) {
                std::cout << "No data received for " << consecutive_timeouts 
                         << " consecutive attempts. Still waiting...\n";
                consecutive_timeouts = 0; // Reset to avoid spam
            }
        }
        
        // Small delay to prevent tight loop
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Cleanup
    transport->Disconnect();
    
    if (log_file.is_open()) {
        log_file.close();
        std::cout << "Data saved to " << output_file << "\n";
    }
    
    // Print final statistics
    monitor.PrintFinalStats();
    
    std::cout << "Receiver finished.\n";
    
    return 0;
}