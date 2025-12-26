#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include "../../include/delila/core/MinimalEventData.hpp"
#include "../../include/delila/core/EventData.hpp"

using DELILA::Digitizer::MinimalEventData;
using DELILA::Digitizer::EventData;

// Benchmark MinimalEventData creation (stack allocation)
static void BM_MinimalEventDataCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        events->reserve(state.range(0));
        
        for (int i = 0; i < state.range(0); ++i) {
            events->push_back(std::make_unique<MinimalEventData>(
                static_cast<uint8_t>(i % 256),      // module
                static_cast<uint8_t>(i % 64),       // channel  
                static_cast<double>(i * 1000.0),    // timeStampNs
                static_cast<uint16_t>(i * 10),      // energy
                static_cast<uint16_t>(i * 5),       // energyShort
                static_cast<uint64_t>(i % 4)        // flags
            ));
        }
        benchmark::DoNotOptimize(events);
    }
    
    // Set bytes processed (22 bytes per MinimalEventData)
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MinimalEventDataCreation)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark EventData creation (heap allocation with vectors)
static void BM_EventDataCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->reserve(state.range(0));
        
        for (int i = 0; i < state.range(0); ++i) {
            auto event = std::make_unique<EventData>(0); // No waveform for fair comparison
            event->module = static_cast<uint8_t>(i % 256);
            event->channel = static_cast<uint8_t>(i % 64);
            event->timeStampNs = static_cast<double>(i * 1000.0);
            event->energy = static_cast<uint16_t>(i * 10);
            event->energyShort = static_cast<uint16_t>(i * 5);
            event->flags = static_cast<uint64_t>(i % 4);
            events->push_back(std::move(event));
        }
        benchmark::DoNotOptimize(events);
    }
    
    // Set bytes processed (EventData is larger due to vectors even when empty)
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(EventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_EventDataCreation)->Range(8, 8192)->Unit(benchmark::kMicrosecond);

// Benchmark memory footprint comparison
static void BM_MinimalEventDataMemoryFootprint(benchmark::State& state) {
    const size_t count = state.range(0);
    
    for (auto _ : state) {
        // Allocate MinimalEventData objects
        auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            events->push_back(std::make_unique<MinimalEventData>(
                static_cast<uint8_t>(i), static_cast<uint8_t>(i), 
                static_cast<double>(i), static_cast<uint16_t>(i), 
                static_cast<uint16_t>(i), static_cast<uint64_t>(i)
            ));
        }
        
        // Measure actual memory usage
        size_t total_memory = sizeof(std::vector<std::unique_ptr<MinimalEventData>>) +
                             count * sizeof(std::unique_ptr<MinimalEventData>) +
                             count * sizeof(MinimalEventData);
                             
        benchmark::DoNotOptimize(events);
        state.counters["MemoryPerEvent"] = benchmark::Counter(
            static_cast<double>(total_memory) / count, 
            benchmark::Counter::kDefaults, 
            benchmark::Counter::OneK::kIs1024
        );
    }
    
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_MinimalEventDataMemoryFootprint)->Range(1000, 10000);

// Benchmark EventData memory footprint (without waveforms)
static void BM_EventDataMemoryFootprint(benchmark::State& state) {
    const size_t count = state.range(0);
    
    for (auto _ : state) {
        // Allocate EventData objects  
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            auto event = std::make_unique<EventData>(0); // No waveform
            event->module = static_cast<uint8_t>(i);
            event->channel = static_cast<uint8_t>(i);
            event->timeStampNs = static_cast<double>(i);
            event->energy = static_cast<uint16_t>(i);
            event->energyShort = static_cast<uint16_t>(i);
            event->flags = static_cast<uint64_t>(i);
            events->push_back(std::move(event));
        }
        
        // Measure actual memory usage (EventData includes vectors)
        size_t total_memory = sizeof(std::vector<std::unique_ptr<EventData>>) +
                             count * sizeof(std::unique_ptr<EventData>) +
                             count * sizeof(EventData);
                             
        benchmark::DoNotOptimize(events);
        state.counters["MemoryPerEvent"] = benchmark::Counter(
            static_cast<double>(total_memory) / count,
            benchmark::Counter::kDefaults, 
            benchmark::Counter::OneK::kIs1024
        );
    }
    
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_EventDataMemoryFootprint)->Range(1000, 10000);

// Benchmark copy performance comparison
static void BM_MinimalEventDataCopy(benchmark::State& state) {
    // Create source data
    auto source_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    source_events->reserve(state.range(0));
    
    for (int i = 0; i < state.range(0); ++i) {
        source_events->push_back(std::make_unique<MinimalEventData>(
            static_cast<uint8_t>(i), static_cast<uint8_t>(i), 
            static_cast<double>(i), static_cast<uint16_t>(i), 
            static_cast<uint16_t>(i), static_cast<uint64_t>(i)
        ));
    }
    
    for (auto _ : state) {
        // Copy all events
        auto copied_events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        copied_events->reserve(source_events->size());
        
        for (const auto& event : *source_events) {
            copied_events->push_back(std::make_unique<MinimalEventData>(*event));
        }
        
        benchmark::DoNotOptimize(copied_events);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MinimalEventDataCopy)->Range(100, 10000)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();