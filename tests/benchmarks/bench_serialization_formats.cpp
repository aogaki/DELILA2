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

// Helper function to create test EventData (without waveforms)
std::unique_ptr<std::vector<std::unique_ptr<EventData>>> 
CreateTestEventData(size_t count) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    events->reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto event = std::make_unique<EventData>(0); // No waveform for fair comparison
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

// Benchmark MinimalEventData serialization (encoding)
static void BM_MinimalEventDataSerialization(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events);
        benchmark::DoNotOptimize(encoded);
    }
    
    // Calculate throughput
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    // Add custom counter for serialized size
    auto sample_encoded = serializer.Encode(events);
    if (sample_encoded) {
        state.counters["SerializedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0),
            benchmark::Counter::kDefaults
        );
    }
}
BENCHMARK(BM_MinimalEventDataSerialization)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark EventData serialization (encoding)  
static void BM_EventDataSerialization(benchmark::State& state) {
    auto events = CreateTestEventData(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        auto encoded = serializer.Encode(events);
        benchmark::DoNotOptimize(encoded);
    }
    
    // Calculate throughput (approximate EventData size without waveforms)
    size_t approx_eventdata_size = sizeof(EventData) + 6 * sizeof(uint32_t); // Vector sizes
    state.SetBytesProcessed(state.iterations() * state.range(0) * approx_eventdata_size);
    state.SetItemsProcessed(state.iterations() * state.range(0));
    
    // Add custom counter for serialized size
    auto sample_encoded = serializer.Encode(events);
    if (sample_encoded) {
        state.counters["SerializedSizePerEvent"] = benchmark::Counter(
            static_cast<double>(sample_encoded->size()) / state.range(0),
            benchmark::Counter::kDefaults
        );
    }
}
BENCHMARK(BM_EventDataSerialization)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark MinimalEventData deserialization (decoding)
static void BM_MinimalEventDataDeserialization(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    
    // Pre-encode the data
    auto encoded = serializer.Encode(events, 42);
    
    for (auto _ : state) {
        auto [decoded_events, seq] = serializer.DecodeMinimalEventData(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MinimalEventDataDeserialization)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark EventData deserialization (decoding)
static void BM_EventDataDeserialization(benchmark::State& state) {
    auto events = CreateTestEventData(state.range(0));
    Serializer serializer;
    
    // Pre-encode the data
    auto encoded = serializer.Encode(events, 42);
    
    for (auto _ : state) {
        auto [decoded_events, seq] = serializer.Decode(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    size_t approx_eventdata_size = sizeof(EventData) + 6 * sizeof(uint32_t);
    state.SetBytesProcessed(state.iterations() * state.range(0) * approx_eventdata_size);
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_EventDataDeserialization)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark round-trip performance (encode + decode)
static void BM_MinimalEventDataRoundTrip(benchmark::State& state) {
    auto events = CreateTestMinimalEvents(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        // Encode
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
        
        // Decode
        auto [decoded_events, seq] = serializer.DecodeMinimalEventData(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData) * 2); // 2x for round-trip
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MinimalEventDataRoundTrip)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark EventData round-trip performance
static void BM_EventDataRoundTrip(benchmark::State& state) {
    auto events = CreateTestEventData(state.range(0));
    Serializer serializer;
    
    for (auto _ : state) {
        // Encode
        auto encoded = serializer.Encode(events, 42);
        benchmark::DoNotOptimize(encoded);
        
        // Decode  
        auto [decoded_events, seq] = serializer.Decode(encoded);
        benchmark::DoNotOptimize(decoded_events);
        benchmark::DoNotOptimize(seq);
    }
    
    size_t approx_eventdata_size = sizeof(EventData) + 6 * sizeof(uint32_t);
    state.SetBytesProcessed(state.iterations() * state.range(0) * approx_eventdata_size * 2); // 2x for round-trip
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_EventDataRoundTrip)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark serialized data size comparison
static void BM_SerializedSizeComparison(benchmark::State& state) {
    const size_t event_count = 1000;
    
    // Create events
    auto minimal_events = CreateTestMinimalEvents(event_count);
    auto eventdata_events = CreateTestEventData(event_count);
    
    Serializer serializer;
    
    for (auto _ : state) {
        // Encode both formats
        auto encoded_minimal = serializer.Encode(minimal_events);
        auto encoded_eventdata = serializer.Encode(eventdata_events);
        
        benchmark::DoNotOptimize(encoded_minimal);
        benchmark::DoNotOptimize(encoded_eventdata);
        
        // Calculate size difference
        if (encoded_minimal && encoded_eventdata) {
            double size_reduction = 1.0 - (static_cast<double>(encoded_minimal->size()) / 
                                          encoded_eventdata->size());
            
            state.counters["MinimalSize"] = benchmark::Counter(encoded_minimal->size());
            state.counters["EventDataSize"] = benchmark::Counter(encoded_eventdata->size());
            state.counters["SizeReductionPercent"] = benchmark::Counter(size_reduction * 100);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * event_count);
}
BENCHMARK(BM_SerializedSizeComparison);

BENCHMARK_MAIN();