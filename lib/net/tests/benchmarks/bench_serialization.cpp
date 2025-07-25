/**
 * @file bench_serialization.cpp
 * @brief Performance benchmarks for BinarySerializer
 * 
 * Tests serialization performance to ensure it meets target throughput:
 * - ≥ 500 MB/s serialization speed (uncompressed)
 * - ≥ 100 MB/s compression speed (LZ4 level 1)
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include "delila/net/serialization/BinarySerializer.hpp"
#include "delila/net/test/TestDataGenerator.hpp"
#include "delila/net/mock/EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Net::Test;
using namespace DELILA;

class SerializationBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& /* state */) override {
        serializer_ = std::make_unique<BinarySerializer>();
        generator_ = std::make_unique<TestDataGenerator>(42); // Fixed seed for reproducibility
    }

    void TearDown(const ::benchmark::State& /* state */) override {
        serializer_.reset();
        generator_.reset();
    }

protected:
    std::unique_ptr<BinarySerializer> serializer_;
    std::unique_ptr<TestDataGenerator> generator_;
};

/**
 * @brief Benchmark small event serialization
 */
BENCHMARK_F(SerializationBenchmark, SmallEventSerialization)(benchmark::State& state) {
    // Generate small events (0-100 samples)
    auto events = generator_->generateEventBatch(100, 0, 100);
    
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
    state.SetLabel("Small events (0-100 samples)");
}

/**
 * @brief Benchmark medium event serialization
 */
BENCHMARK_F(SerializationBenchmark, MediumEventSerialization)(benchmark::State& state) {
    // Generate medium events (500-1000 samples)
    auto events = generator_->generateEventBatch(50, 500, 1000);
    
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
    state.SetLabel("Medium events (500-1000 samples)");
}

/**
 * @brief Benchmark large event serialization (Target: 500+ MB/s)
 */
BENCHMARK_F(SerializationBenchmark, LargeEventSerialization)(benchmark::State& state) {
    // Generate ~10MB of test data
    auto events = generator_->generatePerformanceTestData(10, 100);
    
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
    state.SetLabel("Large events (~10MB target)");
}

/**
 * @brief Benchmark serialization without compression
 */
BENCHMARK_F(SerializationBenchmark, NoCompression)(benchmark::State& state) {
    serializer_->enableCompression(false);
    
    // Generate performance test data
    auto events = generator_->generatePerformanceTestData(5, 100);
    
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
    state.SetLabel("No compression");
}

/**
 * @brief Benchmark LZ4 compression level 1 (Target: 100+ MB/s)
 */
BENCHMARK_F(SerializationBenchmark, CompressionLevel1)(benchmark::State& state) {
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(1);
    
    // Generate large compressible data
    auto events = generator_->generatePerformanceTestData(5, 100);
    
    // Make data more compressible by creating patterns
    for (auto& event : events) {
        event.energy = 1000; // Same energy for all events
        event.energyShort = 500;
    }
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Compression failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("LZ4 compression level 1");
}

/**
 * @brief Benchmark LZ4 compression level 6
 */
BENCHMARK_F(SerializationBenchmark, CompressionLevel6)(benchmark::State& state) {
    serializer_->enableCompression(true);
    serializer_->setCompressionLevel(6);
    
    // Generate compressible data
    auto events = generator_->generatePerformanceTestData(5, 100);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Compression failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("LZ4 compression level 6");
}

/**
 * @brief Benchmark single event serialization
 */
BENCHMARK_F(SerializationBenchmark, SingleEventSerialization)(benchmark::State& state) {
    auto event = generator_->generateSingleEvent(1000); // 1000 samples
    size_t event_bytes = 34 + event.waveform.size() * sizeof(WaveformSample);
    
    for (auto _ : state) {
        auto result = serializer_->encodeEvent(event);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Single event serialization failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * event_bytes);
    state.SetLabel("Single event (1000 samples)");
}

/**
 * @brief Benchmark deserialization performance
 */
BENCHMARK_F(SerializationBenchmark, Deserialization)(benchmark::State& state) {
    // Pre-encode data for deserialization
    auto events = generator_->generateEventBatch(20, 200, 800);
    auto encoded_result = serializer_->encode(events);
    
    if (!isOk(encoded_result)) {
        state.SkipWithError("Failed to pre-encode data");
        return;
    }
    
    const auto& encoded_data = getValue(encoded_result);
    size_t total_bytes = encoded_data.size();
    
    for (auto _ : state) {
        auto result = serializer_->decode(encoded_data);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Deserialization failed");
            return;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("Deserialization");
}

/**
 * @brief Benchmark memory allocation patterns
 */
BENCHMARK_F(SerializationBenchmark, MemoryAllocation)(benchmark::State& state) {
    for (auto _ : state) {
        // Generate fresh data each iteration to test allocation
        auto events = generator_->generateEventBatch(10, 100, 500);
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
        
        if (!isOk(result)) {
            state.SkipWithError("Memory allocation test failed");
            return;
        }
    }
    
    state.SetLabel("Memory allocation patterns");
}

/**
 * @brief Benchmark batch size effects
 */
BENCHMARK_F(SerializationBenchmark, BatchSize100)(benchmark::State& state) {
    auto events = generator_->generateEventBatch(100, 200, 400);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("Batch size 100");
}

BENCHMARK_F(SerializationBenchmark, BatchSize1000)(benchmark::State& state) {
    auto events = generator_->generateEventBatch(1000, 200, 400);
    
    size_t total_bytes = 0;
    for (const auto& event : events) {
        total_bytes += 34 + event.waveform.size() * sizeof(WaveformSample);
    }
    
    for (auto _ : state) {
        auto result = serializer_->encode(events);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("Batch size 1000");
}

// Set benchmark parameters
BENCHMARK_MAIN();