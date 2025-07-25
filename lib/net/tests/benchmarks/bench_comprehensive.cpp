/**
 * @file bench_comprehensive.cpp
 * @brief Comprehensive performance validation benchmarks for Phase 7
 * 
 * These benchmarks provide detailed performance analysis including:
 * - Throughput measurement across different data sizes
 * - Memory usage profiling
 * - CPU utilization analysis
 * - Scalability testing
 * - Production workload simulation
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
#include <iomanip>
#include "delila/net/serialization/BinarySerializer.hpp"
#include "delila/net/test/TestDataGenerator.hpp"
#include "delila/net/mock/EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Net::Test;
using namespace DELILA;

class ComprehensiveBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& /* state */) override {
        serializer_ = std::make_unique<BinarySerializer>();
        generator_ = std::make_unique<TestDataGenerator>(42); // Fixed seed
    }

    void TearDown(const ::benchmark::State& /* state */) override {
        serializer_.reset();
        generator_.reset();
    }

protected:
    std::unique_ptr<BinarySerializer> serializer_;
    std::unique_ptr<TestDataGenerator> generator_;
    
    // Helper to measure memory usage during operation
    struct MemoryStats {
        size_t peak_usage = 0;
        size_t current_usage = 0;
        size_t allocations = 0;
    };
    
    // Estimate memory usage (simplified)
    size_t estimateMemoryUsage(const std::vector<EventData>& events) {
        size_t total = 0;
        for (const auto& event : events) {
            total += sizeof(event) + event.waveform.size() * sizeof(WaveformSample);
        }
        return total;
    }
};

/**
 * @brief Benchmark throughput scaling with data size
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, ThroughputScaling)(benchmark::State& state) {
    size_t data_size_mb = state.range(0);
    auto events = generator_->generatePerformanceTestData(data_size_mb, 100);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Serialization failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("Data size: " + std::to_string(data_size_mb) + "MB");
    
    // Calculate and report throughput using state counters
    state.SetItemsProcessed(state.iterations());
    // Throughput calculated automatically by benchmark framework
    state.counters["Events"] = events.size();
    state.counters["AvgEventSize_KB"] = (total_bytes / events.size()) / 1024.0;
}

// Register throughput scaling benchmarks for different data sizes
BENCHMARK_REGISTER_F(ComprehensiveBenchmark, ThroughputScaling)
    ->Arg(1)->Arg(5)->Arg(10)->Arg(25)->Arg(50)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark compression ratio vs performance trade-off
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, CompressionTradeoff)(benchmark::State& state) {
    int compression_level = state.range(0);
    
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(compression_level);
    
    // Generate compressible data
    auto events = generator_->generatePerformanceTestData(5, 100);
    
    // Make data more compressible
    for (auto& event : events) {
        event.energy = 1000 + (event.energy % 100); // Limited range
        event.energyShort = 500 + (event.energyShort % 50);
    }
    
    size_t uncompressed_bytes = 0;
    size_t compressed_bytes = 0;
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Compression failed");
            return;
        }
        
        const auto& data = getValue(result);
        compressed_bytes = data.size();
        
        // Calculate uncompressed size for first iteration
        if (uncompressed_bytes == 0) {
            for (const auto& event : events) {
                uncompressed_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
            }
            uncompressed_bytes += 64; // Header
        }
    }
    
    state.SetBytesProcessed(state.iterations() * uncompressed_bytes);
    state.SetLabel("Level " + std::to_string(compression_level));
    
    double compression_ratio = (double)uncompressed_bytes / compressed_bytes;
    
    state.counters["CompressionRatio"] = compression_ratio;
    // Throughput calculated automatically by benchmark framework
    state.counters["CompressedSize_KB"] = compressed_bytes / 1024.0;
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, CompressionTradeoff)
    ->Arg(1)->Arg(3)->Arg(6)->Arg(9)->Arg(12)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark event size vs performance relationship
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, EventSizeScaling)(benchmark::State& state) {
    uint32_t waveform_size = state.range(0);
    const size_t num_events = 100;
    
    auto events = generator_->generateEventBatch(num_events, waveform_size, waveform_size);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Serialization failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("Waveform: " + std::to_string(waveform_size) + " samples");
    
    state.counters["EventSize_KB"] = (total_bytes / num_events) / 1024.0;
    state.counters["WaveformSize"] = waveform_size;
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, EventSizeScaling)
    ->Arg(0)->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(5000)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark batch size vs performance
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, BatchSizeScaling)(benchmark::State& state) {
    size_t batch_size = state.range(0);
    const uint32_t waveform_size = 200;
    
    auto events = generator_->generateEventBatch(batch_size, waveform_size, waveform_size);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Serialization failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("Batch: " + std::to_string(batch_size) + " events");
    
    state.counters["BatchSize"] = batch_size;
    state.counters["MemoryUsage_MB"] = estimateMemoryUsage(events) / (1024.0 * 1024.0);
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, BatchSizeScaling)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark round-trip performance (serialize + deserialize)
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, RoundTripPerformance)(benchmark::State& state) {
    auto events = generator_->generatePerformanceTestData(5, 100);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        // Serialize
        auto encoded_result = serializer_->encode(events);
        if (!isOk(encoded_result)) {
            state.SkipWithError("Encoding failed");
            return;
        }
        
        // Deserialize
        const auto& encoded_data = getValue(encoded_result);
        auto decoded_result = serializer_->decode(encoded_data);
        if (!isOk(decoded_result)) {
            state.SkipWithError("Decoding failed");
            return;
        }
        
        benchmark::DoNotOptimize(decoded_result);
    }
    
    // Round-trip processes data twice
    state.SetBytesProcessed(state.iterations() * total_bytes * 2);
    state.SetLabel("Round-trip");
    
    state.counters["Events"] = events.size();
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, RoundTripPerformance)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark sustained throughput under continuous load
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, SustainedThroughput)(benchmark::State& state) {
    const size_t num_batches = 10;
    std::vector<std::vector<EventData>> batches;
    
    // Pre-generate multiple batches
    for (size_t i = 0; i < num_batches; i++) {
        batches.push_back(generator_->generateEventBatch(100, 200, 400));
    }
    
    size_t total_bytes = 0;
    for (const auto& batch : batches) {
        for (const auto& event : batch) {
            total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
        }
    }
    
    size_t batch_index = 0;
    for (auto _ : state) {
        // Cycle through batches to simulate continuous operation
        const auto& events = batches[batch_index % num_batches];
        batch_index++;
        
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Sustained throughput test failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * (total_bytes / num_batches));
    state.SetLabel("Sustained load");
    
    state.counters["Batches"] = state.iterations();
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, SustainedThroughput)
    ->Iterations(100)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark memory allocation patterns
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, MemoryAllocationPatterns)(benchmark::State& state) {
    const size_t data_size_mb = 5;
    
    for (auto _ : state) {
        // Generate fresh data each iteration to test allocation patterns
        auto events = generator_->generatePerformanceTestData(data_size_mb, 100);
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Memory allocation test failed");
            return;
        }
    }
    
    state.SetLabel("Memory allocation");
    
    // Report memory usage statistics
    size_t estimated_memory = data_size_mb * 1024 * 1024;
    state.counters["EstimatedMemory_MB"] = estimated_memory / (1024.0 * 1024.0);
    state.counters["Iterations"] = state.iterations();
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, MemoryAllocationPatterns)
    ->Iterations(50)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark CPU utilization efficiency
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, CPUUtilizationEfficiency)(benchmark::State& state) {
    auto events = generator_->generatePerformanceTestData(10, 100);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("CPU efficiency test failed");
            return;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("CPU efficiency");
    
    state.counters["TotalTime_us"] = duration.count();
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, CPUUtilizationEfficiency)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Production workload simulation
 */
BENCHMARK_DEFINE_F(ComprehensiveBenchmark, ProductionWorkloadSimulation)(benchmark::State& state) {
    // Simulate realistic DAQ workload patterns
    std::vector<std::vector<EventData>> workload_batches;
    
    // Mix of different event sizes (realistic DAQ scenario)
    workload_batches.push_back(generator_->generateEventBatch(50, 0, 100));      // Small events
    workload_batches.push_back(generator_->generateEventBatch(30, 500, 1000));   // Medium events
    workload_batches.push_back(generator_->generateEventBatch(10, 2000, 3000));  // Large events
    workload_batches.push_back(generator_->generateEventBatch(100, 200, 300));   // Typical events
    
    size_t total_bytes = 0;
    for (const auto& batch : workload_batches) {
        for (const auto& event : batch) {
            total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
        }
    }
    
    size_t batch_index = 0;
    for (auto _ : state) {
        // Cycle through different workload patterns
        const auto& events = workload_batches[batch_index % workload_batches.size()];
        batch_index++;
        
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Production workload simulation failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * (total_bytes / workload_batches.size()));
    state.SetLabel("Production workload");
    
    state.counters["WorkloadPatterns"] = workload_batches.size();
}

BENCHMARK_REGISTER_F(ComprehensiveBenchmark, ProductionWorkloadSimulation)
    ->Iterations(200)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();