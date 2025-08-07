#include <benchmark/benchmark.h>
#include "../../lib/net/include/ZMQTransport.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"
#include "../../include/delila/core/EventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::MinimalEventData;
using DELILA::Digitizer::EventData;

// Helper function to create test MinimalEventData
std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>> CreateTestMinimalEvents(size_t count) {
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
std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEventData(size_t count) {
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    events->reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto event = std::make_unique<EventData>(0); // No waveform for comparison
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

// Setup transport for benchmarking (in-memory/local only)
void SetupTransport(ZMQTransport& transport, const std::string& address, const std::string& pattern, bool bind) {
    TransportConfig config;
    config.data_address = address;
    config.data_pattern = pattern;
    config.bind_data = bind;
    
    if (!transport.Configure(config) || !transport.Connect()) {
        throw std::runtime_error("Failed to setup transport for benchmark");
    }
}

// Benchmark MinimalEventData serialization preparation (what gets sent over network)
static void BM_ZMQTransport_MinimalEventData_SerializationPrep(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_minimal", "PUB", true);
    
    auto events = CreateTestMinimalEvents(state.range(0));
    
    for (auto _ : state) {
        // This tests the serialization preparation (what SendMinimal does internally)
        // The actual ZMQ send is mocked by using a real transport but measuring serialization time
        bool result = transport.SendMinimal(events);
        benchmark::DoNotOptimize(result);
    }
    
    // Calculate throughput metrics
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
}
BENCHMARK(BM_ZMQTransport_MinimalEventData_SerializationPrep)
    ->Range(8, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime();

// Benchmark EventData serialization preparation for comparison  
static void BM_ZMQTransport_EventData_SerializationPrep(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_eventdata", "PUB", true);
    
    auto events = CreateTestEventData(state.range(0));
    
    for (auto _ : state) {
        bool result = transport.Send(events);
        benchmark::DoNotOptimize(result);
    }
    
    // Calculate throughput metrics
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(EventData));
}
BENCHMARK(BM_ZMQTransport_EventData_SerializationPrep)
    ->Range(8, 1024)
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime();

// Benchmark data format detection overhead
static void BM_ZMQTransport_FormatDetection(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_detection", "SUB", false);
    
    for (auto _ : state) {
        // Test format detection (will return UNKNOWN since no data is available)
        auto data_type = transport.PeekDataType();
        benchmark::DoNotOptimize(data_type);
    }
}
BENCHMARK(BM_ZMQTransport_FormatDetection)
    ->Unit(benchmark::kNanosecond)
    ->UseRealTime();

// Benchmark memory footprint comparison
static void BM_MinimalEventData_MemoryFootprint(benchmark::State& state) {
    const size_t count = state.range(0);
    
    for (auto _ : state) {
        auto events = CreateTestMinimalEvents(count);
        
        // Calculate memory footprint
        size_t total_memory = sizeof(std::vector<std::unique_ptr<MinimalEventData>>) +
                             count * (sizeof(std::unique_ptr<MinimalEventData>) + sizeof(MinimalEventData));
                             
        benchmark::DoNotOptimize(events);
        state.counters["MemoryPerEvent"] = benchmark::Counter(
            static_cast<double>(total_memory) / count, 
            benchmark::Counter::kDefaults, 
            benchmark::Counter::OneK::kIs1024
        );
    }
    
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_MinimalEventData_MemoryFootprint)->Range(1000, 10000);

static void BM_EventData_MemoryFootprint(benchmark::State& state) {
    const size_t count = state.range(0);
    
    for (auto _ : state) {
        auto events = CreateTestEventData(count);
        
        // Calculate memory footprint (EventData has internal vectors even when empty)
        size_t total_memory = sizeof(std::vector<std::unique_ptr<EventData>>) +
                             count * (sizeof(std::unique_ptr<EventData>) + sizeof(EventData));
                             
        benchmark::DoNotOptimize(events);
        state.counters["MemoryPerEvent"] = benchmark::Counter(
            static_cast<double>(total_memory) / count,
            benchmark::Counter::kDefaults, 
            benchmark::Counter::OneK::kIs1024
        );
    }
    
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_EventData_MemoryFootprint)->Range(1000, 10000);

BENCHMARK_MAIN();