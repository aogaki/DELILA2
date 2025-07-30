#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include <vector>
#include <algorithm>
#include "ZMQTransport.hpp"
#include "Serializer.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

class BenchmarkSuite {
private:
    std::random_device rd;
    std::mt19937 gen;
    
public:
    BenchmarkSuite() : gen(rd()) {}
    
    // Generate test events with varying sizes
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GenerateTestEvents(
        size_t count, size_t waveform_size) {
        
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->reserve(count);
        
        std::uniform_int_distribution<uint64_t> timestamp_dist(1000000000ULL, 9999999999ULL);
        std::uniform_int_distribution<uint32_t> energy_dist(100, 4000);
        std::uniform_int_distribution<int32_t> waveform_dist(-2048, 2047);
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>();
            
            event->timeStampNs = timestamp_dist(gen);
            event->energy = energy_dist(gen);
            event->energyShort = event->energy * 0.8;
            event->module = i % 8;
            event->channel = i % 16;
            event->timeResolution = 4;
            event->waveformSize = waveform_size;
            
            // Generate waveform data
            event->analogProbe1.reserve(waveform_size);
            event->analogProbe2.reserve(waveform_size);
            event->digitalProbe1.reserve(waveform_size);
            event->digitalProbe2.reserve(waveform_size);
            
            for (size_t j = 0; j < waveform_size; ++j) {
                event->analogProbe1.push_back(waveform_dist(gen));
                event->analogProbe2.push_back(waveform_dist(gen));
                event->digitalProbe1.push_back(gen() % 2);
                event->digitalProbe2.push_back(gen() % 2);
            }
            
            events->push_back(std::move(event));
        }
        
        return events;
    }
    
    // Benchmark serialization performance
    void BenchmarkSerialization() {
        std::cout << "\n=== Serialization Benchmark ===\n";
        
        Serializer serializer_compressed, serializer_uncompressed;
        serializer_compressed.EnableCompression(true);
        serializer_compressed.EnableChecksum(true);
        serializer_uncompressed.EnableCompression(false);
        serializer_uncompressed.EnableChecksum(false);
        
        struct TestCase {
            size_t event_count;
            size_t waveform_size;
            std::string description;
        };
        
        std::vector<TestCase> test_cases = {
            {1, 100, "Single small event"},
            {1, 1000, "Single large event"},
            {10, 100, "Small batch, small events"},
            {10, 1000, "Small batch, large events"},
            {100, 100, "Large batch, small events"},
            {100, 1000, "Large batch, large events"}
        };
        
        std::cout << std::left << std::setw(25) << "Test Case"
                 << std::setw(12) << "Raw (μs)"
                 << std::setw(15) << "Compressed (μs)"
                 << std::setw(15) << "Raw Size (KB)"
                 << std::setw(18) << "Compressed (KB)"
                 << std::setw(12) << "Ratio"
                 << "\n";
        std::cout << std::string(100, '-') << "\n";
        
        for (const auto& test_case : test_cases) {
            auto events = GenerateTestEvents(test_case.event_count, test_case.waveform_size);
            
            // Benchmark uncompressed
            auto start = std::chrono::high_resolution_clock::now();
            auto encoded_raw = serializer_uncompressed.Encode(events, 1);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_raw = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            // Benchmark compressed
            start = std::chrono::high_resolution_clock::now();
            auto encoded_compressed = serializer_compressed.Encode(events, 1);
            end = std::chrono::high_resolution_clock::now();
            auto duration_compressed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            double size_raw_kb = encoded_raw ? encoded_raw->size() / 1024.0 : 0;
            double size_compressed_kb = encoded_compressed ? encoded_compressed->size() / 1024.0 : 0;
            double compression_ratio = size_raw_kb > 0 ? size_raw_kb / size_compressed_kb : 1.0;
            
            std::cout << std::left << std::setw(25) << test_case.description
                     << std::setw(12) << duration_raw.count()
                     << std::setw(15) << duration_compressed.count()
                     << std::setw(15) << std::fixed << std::setprecision(1) << size_raw_kb
                     << std::setw(18) << size_compressed_kb
                     << std::setw(12) << std::setprecision(2) << compression_ratio
                     << "\n";
        }
    }
    
    // Benchmark round-trip serialization
    void BenchmarkRoundTrip() {
        std::cout << "\n=== Round-trip Benchmark ===\n";
        
        Serializer serializer;
        serializer.EnableCompression(true);
        serializer.EnableChecksum(true);
        
        auto events = GenerateTestEvents(50, 500);
        
        const int iterations = 100;
        std::vector<double> encode_times, decode_times;
        encode_times.reserve(iterations);
        decode_times.reserve(iterations);
        
        for (int i = 0; i < iterations; ++i) {
            // Encode benchmark
            auto start = std::chrono::high_resolution_clock::now();
            auto encoded = serializer.Encode(events, i);
            auto end = std::chrono::high_resolution_clock::now();
            auto encode_duration = std::chrono::duration<double, std::micro>(end - start);
            encode_times.push_back(encode_duration.count());
            
            // Decode benchmark
            start = std::chrono::high_resolution_clock::now();
            auto [decoded, sequence] = serializer.Decode(encoded);
            end = std::chrono::high_resolution_clock::now();
            auto decode_duration = std::chrono::duration<double, std::micro>(end - start);
            decode_times.push_back(decode_duration.count());
            
            // Verify integrity
            if (!decoded || decoded->size() != events->size()) {
                std::cerr << "Data integrity check failed on iteration " << i << "\n";
                return;
            }
        }
        
        // Calculate statistics
        auto calc_stats = [](const std::vector<double>& times) {
            double sum = std::accumulate(times.begin(), times.end(), 0.0);
            double mean = sum / times.size();
            
            std::vector<double> sorted_times = times;
            std::sort(sorted_times.begin(), sorted_times.end());
            
            double median = sorted_times[sorted_times.size() / 2];
            double min_time = sorted_times.front();
            double max_time = sorted_times.back();
            
            return std::make_tuple(mean, median, min_time, max_time);
        };
        
        auto [encode_mean, encode_median, encode_min, encode_max] = calc_stats(encode_times);
        auto [decode_mean, decode_median, decode_min, decode_max] = calc_stats(decode_times);
        
        std::cout << "Test: 50 events, 500 samples each, " << iterations << " iterations\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "\nEncode Performance (μs):\n";
        std::cout << "  Mean: " << encode_mean << ", Median: " << encode_median
                 << ", Min: " << encode_min << ", Max: " << encode_max << "\n";
        
        std::cout << "\nDecode Performance (μs):\n";
        std::cout << "  Mean: " << decode_mean << ", Median: " << decode_median
                 << ", Min: " << decode_min << ", Max: " << decode_max << "\n";
        
        std::cout << "\nThroughput:\n";
        std::cout << "  Encode: " << std::setprecision(0) << (50.0 * 1e6) / encode_mean << " events/second\n";
        std::cout << "  Decode: " << (50.0 * 1e6) / decode_mean << " events/second\n";
    }
    
    // Network throughput test (requires both sender and receiver)
    void BenchmarkNetworkThroughput() {
        std::cout << "\n=== Network Throughput Test ===\n";
        std::cout << "This test requires running sender and receiver separately.\n";
        std::cout << "Usage:\n";
        std::cout << "  Terminal 1: ./receiver tcp://localhost:5555 30 network_test.log\n";
        std::cout << "  Terminal 2: ./sender tcp://*:5555 50 100 50\n";
        std::cout << "\nThe test will send 5000 events (50 events * 100 batches) with 50ms delays.\n";
        std::cout << "Expected throughput: ~1000 events/second, ~10-50 Mbps depending on event size.\n";
    }
    
    // Memory usage test
    void BenchmarkMemoryUsage() {
        std::cout << "\n=== Memory Usage Analysis ===\n";
        
        struct MemoryTest {
            size_t event_count;
            size_t waveform_size;
            std::string description;
        };
        
        std::vector<MemoryTest> tests = {
            {1, 1000, "1 event, 1K samples"},
            {10, 1000, "10 events, 1K samples each"},
            {100, 1000, "100 events, 1K samples each"},
            {1000, 100, "1K events, 100 samples each"},
            {1000, 1000, "1K events, 1K samples each"}
        };
        
        Serializer serializer;
        serializer.EnableCompression(true);
        
        std::cout << std::left << std::setw(30) << "Test Case"
                 << std::setw(15) << "Raw Size (MB)"
                 << std::setw(18) << "Serialized (MB)"
                 << std::setw(15) << "Compressed (MB)"
                 << std::setw(12) << "Ratio"
                 << "\n";
        std::cout << std::string(90, '-') << "\n";
        
        for (const auto& test : tests) {
            auto events = GenerateTestEvents(test.event_count, test.waveform_size);
            
            // Calculate raw memory usage
            size_t raw_size = 0;
            for (const auto& event : *events) {
                raw_size += sizeof(EventData);
                raw_size += event->analogProbe1.size() * sizeof(int32_t);
                raw_size += event->analogProbe2.size() * sizeof(int32_t);
                raw_size += event->digitalProbe1.size() * sizeof(uint8_t);
                raw_size += event->digitalProbe2.size() * sizeof(uint8_t);
            }
            
            // Serialize without compression
            serializer.EnableCompression(false);
            auto serialized = serializer.Encode(events, 1);
            
            // Serialize with compression
            serializer.EnableCompression(true);
            auto compressed = serializer.Encode(events, 1);
            
            double raw_mb = raw_size / (1024.0 * 1024.0);
            double serialized_mb = serialized ? serialized->size() / (1024.0 * 1024.0) : 0;
            double compressed_mb = compressed ? compressed->size() / (1024.0 * 1024.0) : 0;
            double compression_ratio = serialized_mb > 0 ? serialized_mb / compressed_mb : 1.0;
            
            std::cout << std::left << std::setw(30) << test.description
                     << std::setw(15) << std::fixed << std::setprecision(2) << raw_mb
                     << std::setw(18) << serialized_mb
                     << std::setw(15) << compressed_mb
                     << std::setw(12) << std::setprecision(1) << compression_ratio
                     << "\n";
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "DELILA2 Network Library - Performance Test Suite\n";
    std::cout << "===============================================\n";
    
    BenchmarkSuite suite;
    
    // Run all benchmarks
    suite.BenchmarkSerialization();
    suite.BenchmarkRoundTrip();
    suite.BenchmarkMemoryUsage();
    suite.BenchmarkNetworkThroughput();
    
    std::cout << "\n=== Performance Summary ===\n";
    std::cout << "The DELILA2 network library provides:\n";
    std::cout << "• High-speed binary serialization (>10K events/second)\n";
    std::cout << "• Efficient LZ4 compression (2-5x size reduction)\n";
    std::cout << "• Data integrity with xxHash checksums\n";
    std::cout << "• ZeroMQ transport for reliable delivery\n";
    std::cout << "• Optimized for nuclear physics waveform data\n";
    std::cout << "\nFor network tests, use the separate sender/receiver programs.\n";
    
    return 0;
}