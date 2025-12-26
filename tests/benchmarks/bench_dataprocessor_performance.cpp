#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include "../../lib/net/include/DataProcessor.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"
#include "../../include/delila/core/EventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::MinimalEventData;
using DELILA::Digitizer::EventData;

// Helper function to create test MinimalEventData
std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> 
CreateTestMinimalEvents(size_t count) {
    auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    events->reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        events->push_back(std::make_unique<MinimalEventData>(
            static_cast<uint8_t>(i % 256),      // module
            static_cast<uint8_t>(i % 64),       // channel  
            static_cast<double>(i * 1000.0),    // timeStampNs
            static_cast<uint16_t>(i * 10),      // energy
            static_cast<uint16_t>(i * 5),       // energyShort
            static_cast<uint64_t>(i % 4)        // flags
        ));
    }
    return events;
}

// Helper function to create test EventData (without waveforms for baseline)
std::unique_ptr<std::vector<std::unique_ptr<EventData>>> 
CreateTestEventData(size_t count) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    events->reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto event = std::make_unique<EventData>(0); // No waveform for baseline
        event->module = static_cast<uint8_t>(i % 256);
        event->channel = static_cast<uint8_t>(i % 64);
        event->timeStampNs = static_cast<double>(i * 1000.0);
        event->energy = static_cast<uint16_t>(i * 10);
        event->energyShort = static_cast<uint16_t>(i * 5);
        event->flags = static_cast<uint64_t>(i % 4);
        events->push_back(std::move(event));
    }
    return events;
}

// Helper function to create test EventData with waveforms
std::unique_ptr<std::vector<std::unique_ptr<EventData>>> 
CreateTestEventDataWithWaveforms(size_t count, size_t waveform_size = 512) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    events->reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto event = std::make_unique<EventData>(waveform_size);
        event->module = static_cast<uint8_t>(i % 256);
        event->channel = static_cast<uint8_t>(i % 64);
        event->timeStampNs = static_cast<double>(i * 1000.0);
        event->energy = static_cast<uint16_t>(i * 10);
        event->energyShort = static_cast<uint16_t>(i * 5);
        event->flags = static_cast<uint64_t>(i % 4);
        
        // Fill analog probes with test data
        event->analogProbe1.resize(waveform_size);
        event->analogProbe2.resize(waveform_size / 2); // Different sizes for realism
        for (size_t j = 0; j < waveform_size; ++j) {
            event->analogProbe1[j] = static_cast<int32_t>((i + j) % 4096);
        }
        for (size_t j = 0; j < waveform_size / 2; ++j) {
            event->analogProbe2[j] = static_cast<int32_t>((i + j + 1000) % 2048);
        }
        
        events->push_back(std::move(event));
    }
    return events;
}

// ====================================================================
// DATAPROCESSOR PERFORMANCE BENCHMARKS
// ====================================================================

// MinimalEventData - Process (encoding) performance
static void BM_DataProcessor_MinimalEventData_Process(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    DataProcessor processor;
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    // Add size metrics
    auto sample_encoded = processor.Process(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0)
        );
        state.counters["CompressionRatio"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / (state.range(0) * sizeof(MinimalEventData))
        );
    }
}
BENCHMARK(BM_DataProcessor_MinimalEventData_Process)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// MinimalEventData - Decode performance  
static void BM_DataProcessor_MinimalEventData_Decode(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    DataProcessor processor;
    auto encoded = processor.Process(events, 42);
    
    for (auto _ : state) {
        auto [decoded_events, seq] = processor.DecodeMinimal(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DataProcessor_MinimalEventData_Decode)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// MinimalEventData - Round-trip performance
static void BM_DataProcessor_MinimalEventData_RoundTrip(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    DataProcessor processor;
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
        
        auto [decoded_events, seq] = processor.DecodeMinimal(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData) * 2);
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DataProcessor_MinimalEventData_RoundTrip)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// EventData - Process performance (no waveforms)
static void BM_DataProcessor_EventData_Process(benchmark::State& state) {
    auto events = CreateTestEventData(state.range(0));
    DataProcessor processor;
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    size_t approx_eventdata_size = sizeof(EventData) + 6 * sizeof(uint32_t);
    state.SetBytesProcessed(state.iterations() * state.range(0) * approx_eventdata_size);
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    auto sample_encoded = processor.Process(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0)
        );
    }
}
BENCHMARK(BM_DataProcessor_EventData_Process)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// EventData with waveforms - Compression efficiency test
static void BM_DataProcessor_EventData_WithWaveforms(benchmark::State& state) {
    auto events = CreateTestEventDataWithWaveforms(state.range(0), 1024);
    DataProcessor processor;
    // Compression removed - LZ4 is no longer available
    processor.EnableChecksum(true);
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    size_t waveform_size = 1024 * sizeof(int32_t) + 512 * sizeof(int32_t); // probe1 + probe2
    size_t approx_eventdata_size = sizeof(EventData) + 6 * sizeof(uint32_t) + waveform_size;
    state.SetBytesProcessed(state.iterations() * state.range(0) * approx_eventdata_size);
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    auto sample_encoded = processor.Process(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0)
        );
        
        size_t original_size = state.range(0) * approx_eventdata_size;
        state.counters["CompressionRatio"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / original_size
        );
    }
}
BENCHMARK(BM_DataProcessor_EventData_WithWaveforms)->Range(8, 256)->Unit(benchmark::kMicrosecond);

// ====================================================================
// COMPRESSION CONFIGURATION BENCHMARKS
// ====================================================================

// Compression enabled vs disabled comparison
static void BM_DataProcessor_Compression_Enabled(benchmark::State& state) {
    auto events = CreateTestEventDataWithWaveforms(state.range(0), 512);
    DataProcessor processor;
    // Compression removed - LZ4 is no longer available
    processor.EnableChecksum(false); // Focus on compression only
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    auto sample_encoded = processor.Process(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSize"] = benchmark::Counter(sample_encoded->size());
    }
}
BENCHMARK(BM_DataProcessor_Compression_Enabled)->Range(8, 512)->Unit(benchmark::kMicrosecond);

static void BM_DataProcessor_Compression_Disabled(benchmark::State& state) {
    auto events = CreateTestEventDataWithWaveforms(state.range(0), 512);
    DataProcessor processor;
    // Compression removed - no longer available
    processor.EnableChecksum(false); // Focus on compression only
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    auto sample_encoded = processor.Process(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSize"] = benchmark::Counter(sample_encoded->size());
    }
}
BENCHMARK(BM_DataProcessor_Compression_Disabled)->Range(8, 512)->Unit(benchmark::kMicrosecond);

// ====================================================================
// CHECKSUM PERFORMANCE BENCHMARKS (CRC32)
// ====================================================================

// CRC32 checksum enabled
static void BM_DataProcessor_CRC32_Enabled(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    DataProcessor processor;
    // Compression removed - no longer available // Focus on checksum only
    processor.EnableChecksum(true);     // Enable CRC32
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DataProcessor_CRC32_Enabled)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// CRC32 checksum disabled (baseline)
static void BM_DataProcessor_CRC32_Disabled(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    DataProcessor processor;
    // Compression removed - no longer available // Focus on checksum only  
    processor.EnableChecksum(false);    // Disable checksum
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DataProcessor_CRC32_Disabled)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// ====================================================================
// SCALING AND THROUGHPUT BENCHMARKS
// ====================================================================

// Large dataset throughput test
static void BM_DataProcessor_Large_Dataset(benchmark::State& state) {
    const size_t large_count = 10000; // 10K events
    auto events = CreateTestMinimalEvents(large_count);
    DataProcessor processor;
    
    for (auto _ : state) {
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetBytesProcessed(state.iterations() * large_count * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * large_count);
    
    // Calculate events per second using timing counters
    state.counters["EventsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations() * large_count), 
        benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_DataProcessor_Large_Dataset)->Unit(benchmark::kMillisecond);

// Memory allocation efficiency test
static void BM_DataProcessor_Memory_Efficiency(benchmark::State& state) {
    auto events = CreateTestEventDataWithWaveforms(state.range(0), 1024);
    
    for (auto _ : state) {
        DataProcessor processor; // Create new instance each time
        auto encoded = processor.Process(events, 42);
        benchmark::DoNotOptimize(encoded);
        // processor goes out of scope and is destroyed
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_DataProcessor_Memory_Efficiency)->Range(8, 256)->Unit(benchmark::kMicrosecond);

// ====================================================================
// REALISTIC NUCLEAR PHYSICS WORKLOAD SIMULATION
// ====================================================================

// Simulate typical nuclear physics data acquisition burst
static void BM_DataProcessor_Nuclear_Physics_Burst(benchmark::State& state) {
    // Typical nuclear physics: mix of events with and without waveforms
    // 70% minimal events (trigger data), 30% full waveform events
    const size_t total_events = 1000;
    const size_t minimal_count = static_cast<size_t>(total_events * 0.7);
    const size_t waveform_count = total_events - minimal_count;
    
    auto minimal_events = CreateTestMinimalEvents(minimal_count);
    auto waveform_events = CreateTestEventDataWithWaveforms(waveform_count, 2048); // 2K samples
    
    DataProcessor processor;
    // Compression removed - LZ4 is no longer available  // Compression essential for waveforms
    processor.EnableChecksum(true);     // Data integrity crucial
    
    for (auto _ : state) {
        // Process minimal events
        auto encoded_minimal = processor.Process(minimal_events, 42);
        benchmark::DoNotOptimize(encoded_minimal);
        
        // Process waveform events  
        auto encoded_waveforms = processor.Process(waveform_events, 43);
        benchmark::DoNotOptimize(encoded_waveforms);
    }
    
    size_t total_bytes = minimal_count * sizeof(MinimalEventData) + 
                        waveform_count * (sizeof(EventData) + 2048 * sizeof(int32_t));
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetItemsProcessed(state.iterations() * total_events);
    
    // Calculate physics event rate using timing counters
    state.counters["PhysicsEventsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations() * total_events), 
        benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_DataProcessor_Nuclear_Physics_Burst)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();