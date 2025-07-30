#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include "ZMQTransport.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

class RandomEventGenerator {
private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<uint64_t> timestamp_dist;
    std::uniform_int_distribution<uint32_t> energy_dist;
    std::uniform_int_distribution<uint16_t> module_dist;
    std::uniform_int_distribution<uint16_t> channel_dist;
    std::uniform_int_distribution<int32_t> waveform_dist;
    std::uniform_int_distribution<uint8_t> digital_dist;
    std::uniform_int_distribution<size_t> waveform_size_dist;

public:
    RandomEventGenerator() 
        : gen(rd())
        , timestamp_dist(1000000000ULL, 9999999999ULL)  // Realistic timestamps
        , energy_dist(100, 4000)                        // Energy range
        , module_dist(0, 7)                            // 8 modules
        , channel_dist(0, 15)                          // 16 channels per module
        , waveform_dist(-2048, 2047)                   // 12-bit ADC range
        , digital_dist(0, 1)                           // Digital values
        , waveform_size_dist(100, 1000)                // Waveform samples
    {}

    std::unique_ptr<EventData> GenerateEvent() {
        auto event = std::make_unique<EventData>();
        
        // Basic event properties
        event->timeStampNs = timestamp_dist(gen);
        event->energy = energy_dist(gen);
        event->energyShort = event->energy * 0.8; // Short gate energy
        event->module = module_dist(gen);
        event->channel = channel_dist(gen);
        event->timeResolution = 4; // 4 ns resolution
        
        // Probe configurations
        event->analogProbe1Type = 0; // Input signal
        event->analogProbe2Type = 1; // Trigger
        event->digitalProbe1Type = 0;
        event->digitalProbe2Type = 1;
        event->digitalProbe3Type = 2;
        event->digitalProbe4Type = 3;
        event->downSampleFactor = 1;
        
        // Random flags (occasionally set pileup, etc.)
        event->flags = 0;
        if (gen() % 20 == 0) event->flags |= EventData::FLAG_PILEUP;
        if (gen() % 100 == 0) event->flags |= EventData::FLAG_OVER_RANGE;
        
        // Generate waveform data
        size_t waveform_size = waveform_size_dist(gen);
        event->waveformSize = waveform_size;
        
        // Analog waveforms (simulate exponential decay pulse)
        event->analogProbe1.reserve(waveform_size);
        event->analogProbe2.reserve(waveform_size);
        
        int32_t baseline = 100;
        int32_t amplitude = event->energy / 10;
        size_t peak_position = waveform_size / 3;
        
        for (size_t i = 0; i < waveform_size; ++i) {
            // Simulate pulse shape
            int32_t sample;
            if (i < peak_position) {
                // Rising edge
                sample = baseline + (amplitude * i) / peak_position;
            } else {
                // Exponential decay
                double decay = std::exp(-0.1 * (i - peak_position));
                sample = baseline + amplitude * decay;
            }
            
            // Add noise
            sample += (waveform_dist(gen) % 20) - 10;
            
            event->analogProbe1.push_back(sample);
            event->analogProbe2.push_back(sample / 2); // Attenuated version
        }
        
        // Digital waveforms (trigger patterns)
        event->digitalProbe1.reserve(waveform_size);
        event->digitalProbe2.reserve(waveform_size);
        event->digitalProbe3.reserve(waveform_size);
        event->digitalProbe4.reserve(waveform_size);
        
        for (size_t i = 0; i < waveform_size; ++i) {
            // Trigger around peak
            bool trigger = (i >= peak_position - 10 && i <= peak_position + 20);
            event->digitalProbe1.push_back(trigger ? 1 : 0);
            event->digitalProbe2.push_back((i % 10 < 5) ? 1 : 0); // Clock
            event->digitalProbe3.push_back(digital_dist(gen));    // Random
            event->digitalProbe4.push_back(0); // Unused
        }
        
        return event;
    }
    
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GenerateEventBatch(size_t count) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            events->push_back(GenerateEvent());
        }
        
        return events;
    }
};

void PrintStatistics(size_t events_sent, size_t total_bytes, 
                    std::chrono::duration<double> elapsed) {
    double rate_events = events_sent / elapsed.count();
    double rate_mbps = (total_bytes * 8.0) / (elapsed.count() * 1e6);
    double avg_bytes_per_event = static_cast<double>(total_bytes) / events_sent;
    
    std::cout << "\n=== Performance Statistics ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Events sent: " << events_sent << "\n";
    std::cout << "Total bytes: " << total_bytes << " (" << total_bytes/1024.0/1024.0 << " MB)\n";
    std::cout << "Elapsed time: " << elapsed.count() << " seconds\n";
    std::cout << "Event rate: " << rate_events << " events/second\n";
    std::cout << "Data rate: " << rate_mbps << " Mbps\n";
    std::cout << "Average size per event: " << avg_bytes_per_event << " bytes\n";
    std::cout << "===============================\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string address = "tcp://*:5555";
    size_t batch_size = 10;
    size_t num_batches = 100;
    size_t delay_ms = 100;
    
    if (argc > 1) address = argv[1];
    if (argc > 2) batch_size = std::stoul(argv[2]);
    if (argc > 3) num_batches = std::stoul(argv[3]);
    if (argc > 4) delay_ms = std::stoul(argv[4]);
    
    std::cout << "DELILA2 Network Library - Data Sender\n";
    std::cout << "=====================================\n";
    std::cout << "Address: " << address << "\n";
    std::cout << "Batch size: " << batch_size << " events\n";
    std::cout << "Number of batches: " << num_batches << "\n";
    std::cout << "Delay between batches: " << delay_ms << " ms\n";
    std::cout << "Total events to send: " << (batch_size * num_batches) << "\n\n";
    
    // Configure transport as publisher
    TransportConfig config;
    config.data_address = address;
    config.is_publisher = true;
    config.bind_data = true;
    
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
    
    std::cout << "Publisher connected. Waiting 2 seconds for subscribers...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Create event generator
    RandomEventGenerator generator;
    
    // Performance tracking
    size_t total_events_sent = 0;
    size_t total_bytes_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Starting data transmission...\n\n";
    
    // Send data
    for (size_t batch = 0; batch < num_batches; ++batch) {
        // Generate random events
        auto events = generator.GenerateEventBatch(batch_size);
        
        // Send events
        bool success = transport->Send(events);
        
        if (success) {
            total_events_sent += batch_size;
            
            // Estimate size (simplified calculation)
            size_t batch_size_bytes = 0;
            for (const auto& event : *events) {
                batch_size_bytes += sizeof(EventData) + 
                    event->analogProbe1.size() * sizeof(int32_t) +
                    event->analogProbe2.size() * sizeof(int32_t) +
                    event->digitalProbe1.size() * sizeof(uint8_t) +
                    event->digitalProbe2.size() * sizeof(uint8_t) +
                    event->digitalProbe3.size() * sizeof(uint8_t) +
                    event->digitalProbe4.size() * sizeof(uint8_t);
            }
            total_bytes_sent += batch_size_bytes;
            
            // Print progress
            if ((batch + 1) % 10 == 0 || batch == 0) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                    current_time - start_time);
                double rate = total_events_sent / elapsed.count();
                
                std::cout << "Batch " << std::setw(4) << (batch + 1) << "/" << num_batches 
                         << " | Events: " << std::setw(6) << total_events_sent
                         << " | Rate: " << std::setw(8) << std::fixed << std::setprecision(1) 
                         << rate << " events/s\r" << std::flush;
            }
            
        } else {
            std::cerr << "Failed to send batch " << batch << "\n";
        }
        
        // Delay between batches
        if (delay_ms > 0 && batch < num_batches - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    
    std::cout << "\n\nTransmission completed!\n";
    PrintStatistics(total_events_sent, total_bytes_sent, total_elapsed);
    
    // Keep connection alive for a bit
    std::cout << "\nKeeping connection alive for 5 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    transport->Disconnect();
    std::cout << "Sender finished.\n";
    
    return 0;
}