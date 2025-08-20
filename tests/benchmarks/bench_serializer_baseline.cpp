#include <benchmark/benchmark.h>
#include "../../lib/net/include/Serializer.hpp"
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

// Helper function to create test EventData (without waveforms for fair comparison)
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

// Serializer - MinimalEventData encoding (baseline for comparison)
static void BM_Serializer_MinimalEventData_Encode(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    // Add size metrics
    auto sample_encoded = serializer.Encode(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0)
        );
        state.counters["CompressionRatio"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / (state.range(0) * sizeof(MinimalEventData))
        );
    }
}
BENCHMARK(BM_Serializer_MinimalEventData_Encode)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Serializer - MinimalEventData decoding (baseline for comparison)
static void BM_Serializer_MinimalEventData_Decode(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    auto encoded = serializer.Encode(events, 42);
    
    for (auto _ : state) {
        auto [decoded_events, seq] = serializer.DecodeMinimalEventData(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Serializer_MinimalEventData_Decode)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Serializer - MinimalEventData round-trip 
static void BM_Serializer_MinimalEventData_RoundTrip(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
        
        auto [decoded_events, seq] = serializer.DecodeMinimalEventData(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData) * 2);
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Serializer_MinimalEventData_RoundTrip)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Serializer - EventData encoding (baseline)
static void BM_Serializer_EventData_Encode(benchmark::State& state) {
    auto events = CreateTestEventData(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    size_t approx_eventdata_size = sizeof(EventData) + 6 * sizeof(uint32_t);
    state.SetBytesProcessed(state.iterations() * state.range(0) * approx_eventdata_size);
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    auto sample_encoded = serializer.Encode(events, 42);
    if (sample_encoded) {
        state.counters["EncodedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0)
        );
    }
}
BENCHMARK(BM_Serializer_EventData_Encode)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Serializer - xxHash checksum (baseline)
static void BM_Serializer_xxHash_Checksum(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    // Serializer uses xxHash by default
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Serializer_xxHash_Checksum)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Large dataset throughput test with Serializer
static void BM_Serializer_Large_Dataset(benchmark::State& state) {
    const size_t large_count = 10000; // 10K events
    auto events = CreateTestMinimalEvents(large_count);
    Serializer serializer;
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events, 42);
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
BENCHMARK(BM_Serializer_Large_Dataset)->Unit(benchmark::kMillisecond);

// Memory allocation efficiency test
static void BM_Serializer_Memory_Efficiency(benchmark::State& state) {
    auto events = CreateTestEventData(state.range(0));
    
    for (auto _ : state) {
        Serializer serializer; // Create new instance each time
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
        // serializer goes out of scope and is destroyed
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Serializer_Memory_Efficiency)->Range(8, 256)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();