/**
 * @file bench_old_vs_new_api.cpp
 * @brief Benchmark comparing old (deprecated) vs new (separated) serialization API
 * 
 * This benchmark validates that the new separated serialization approach does not
 * introduce performance regression compared to the old integrated approach.
 */

#include <benchmark/benchmark.h>
#include "../../lib/net/include/ZMQTransport.hpp"
#include "../../lib/net/include/Serializer.hpp"
#include "../../include/delila/core/EventData.hpp"
#include "../../include/delila/core/MinimalEventData.hpp"

using namespace DELILA::Net;
using DELILA::Digitizer::EventData;
using DELILA::Digitizer::MinimalEventData;

// Helper function to create test EventData
std::unique_ptr<std::vector<std::unique_ptr<EventData>>> CreateTestEvents(size_t count) {
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

// Setup transport for benchmarking
void SetupTransport(ZMQTransport& transport, const std::string& address) {
    TransportConfig config;
    config.data_address = address;
    config.data_pattern = "PUSH";
    config.bind_data = true;
    config.is_publisher = true;
    
    if (!transport.Configure(config) || !transport.Connect()) {
        throw std::runtime_error("Failed to setup transport for benchmark");
    }
}

// ============================================================================
// OLD API BENCHMARKS (using deprecated methods)
// ============================================================================

// Benchmark old API: Send(EventData) - includes internal serialization
static void BM_OldAPI_SendEventData(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_old_eventdata");
    
    auto events = CreateTestEvents(state.range(0));
    
    for (auto _ : state) {
        // Clone events for each iteration (since Send consumes them)
        auto events_copy = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        for (const auto& event : *events) {
            auto copy = std::make_unique<EventData>(0);
            *copy = *event; // Copy event data
            events_copy->push_back(std::move(copy));
        }
        
        // OLD API: Transport handles serialization internally
        bool result = transport.Send(events_copy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(EventData));
    state.SetLabel("OldAPI-EventData");
}
BENCHMARK(BM_OldAPI_SendEventData)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// Benchmark old API: SendMinimal(MinimalEventData) - includes internal serialization  
static void BM_OldAPI_SendMinimalEventData(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_old_minimal");
    
    auto events = CreateTestMinimalEvents(state.range(0));
    
    for (auto _ : state) {
        // Clone events for each iteration
        auto events_copy = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        for (const auto& event : *events) {
            auto copy = std::make_unique<MinimalEventData>(*event);
            events_copy->push_back(std::move(copy));
        }
        
        // OLD API: Transport handles serialization internally
        bool result = transport.SendMinimal(events_copy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetLabel("OldAPI-MinimalEventData");
}
BENCHMARK(BM_OldAPI_SendMinimalEventData)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// NEW API BENCHMARKS (using separated serialization)
// ============================================================================

// Benchmark new API: External Serializer + SendBytes for EventData
static void BM_NewAPI_SendEventData(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_new_eventdata");
    
    Serializer serializer;
    uint64_t sequence_number = 1;
    
    auto events = CreateTestEvents(state.range(0));
    
    for (auto _ : state) {
        // Clone events for each iteration
        auto events_copy = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        for (const auto& event : *events) {
            auto copy = std::make_unique<EventData>(0);
            *copy = *event;
            events_copy->push_back(std::move(copy));
        }
        
        // NEW API: External serialization + byte transport
        auto serialized_bytes = serializer.Encode(events_copy, sequence_number++);
        if (serialized_bytes) {
            bool result = transport.SendBytes(serialized_bytes);
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(EventData));
    state.SetLabel("NewAPI-EventData");
}
BENCHMARK(BM_NewAPI_SendEventData)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// Benchmark new API: External Serializer + SendBytes for MinimalEventData
static void BM_NewAPI_SendMinimalEventData(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_new_minimal");
    
    Serializer serializer;
    uint64_t sequence_number = 1;
    
    auto events = CreateTestMinimalEvents(state.range(0));
    
    for (auto _ : state) {
        // Clone events for each iteration
        auto events_copy = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        for (const auto& event : *events) {
            auto copy = std::make_unique<MinimalEventData>(*event);
            events_copy->push_back(std::move(copy));
        }
        
        // NEW API: External serialization + byte transport
        auto serialized_bytes = serializer.Encode(events_copy, sequence_number++);
        if (serialized_bytes) {
            bool result = transport.SendBytes(serialized_bytes);
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetLabel("NewAPI-MinimalEventData");
}
BENCHMARK(BM_NewAPI_SendMinimalEventData)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// SERIALIZATION-ONLY BENCHMARKS (isolate serialization performance)
// ============================================================================

// Benchmark just serialization step for EventData
static void BM_Serialization_EventData(benchmark::State& state) {
    Serializer serializer;
    uint64_t sequence_number = 1;
    
    auto events = CreateTestEvents(state.range(0));
    
    for (auto _ : state) {
        // Clone events for each iteration
        auto events_copy = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        for (const auto& event : *events) {
            auto copy = std::make_unique<EventData>(0);
            *copy = *event;
            events_copy->push_back(std::move(copy));
        }
        
        // Only measure serialization
        auto serialized_bytes = serializer.Encode(events_copy, sequence_number++);
        benchmark::DoNotOptimize(serialized_bytes);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(EventData));
    state.SetLabel("SerializationOnly-EventData");
}
BENCHMARK(BM_Serialization_EventData)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// Benchmark just serialization step for MinimalEventData
static void BM_Serialization_MinimalEventData(benchmark::State& state) {
    Serializer serializer;
    uint64_t sequence_number = 1;
    
    auto events = CreateTestMinimalEvents(state.range(0));
    
    for (auto _ : state) {
        // Clone events for each iteration
        auto events_copy = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
        for (const auto& event : *events) {
            auto copy = std::make_unique<MinimalEventData>(*event);
            events_copy->push_back(std::move(copy));
        }
        
        // Only measure serialization
        auto serialized_bytes = serializer.Encode(events_copy, sequence_number++);
        benchmark::DoNotOptimize(serialized_bytes);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(MinimalEventData));
    state.SetLabel("SerializationOnly-MinimalEventData");
}
BENCHMARK(BM_Serialization_MinimalEventData)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// TRANSPORT-ONLY BENCHMARKS (isolate transport performance)
// ============================================================================

// Benchmark just transport step (raw bytes)
static void BM_Transport_RawBytes(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_transport_only");
    
    // Create dummy byte data of realistic size
    const size_t event_count = state.range(0);
    const size_t bytes_per_event = sizeof(EventData);
    const size_t total_bytes = event_count * bytes_per_event;
    
    auto dummy_bytes = std::make_unique<std::vector<uint8_t>>(total_bytes);
    std::fill(dummy_bytes->begin(), dummy_bytes->end(), 0xAB); // Fill with pattern
    
    for (auto _ : state) {
        // Create copy for each iteration
        auto bytes_copy = std::make_unique<std::vector<uint8_t>>(*dummy_bytes);
        
        // Only measure transport
        bool result = transport.SendBytes(bytes_copy);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * event_count);
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetLabel("TransportOnly-RawBytes");
}
BENCHMARK(BM_Transport_RawBytes)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// MEMORY USAGE BENCHMARKS
// ============================================================================

// Benchmark memory allocation patterns - Old API
static void BM_Memory_OldAPI(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_memory_old");
    
    for (auto _ : state) {
        auto events = CreateTestEvents(state.range(0));
        
        // Measure memory allocation overhead of old approach
        bool result = transport.Send(events);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetLabel("Memory-OldAPI");
}
BENCHMARK(BM_Memory_OldAPI)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

// Benchmark memory allocation patterns - New API
static void BM_Memory_NewAPI(benchmark::State& state) {
    ZMQTransport transport;
    SetupTransport(transport, "inproc://benchmark_memory_new");
    
    Serializer serializer;
    uint64_t sequence_number = 1;
    
    for (auto _ : state) {
        auto events = CreateTestEvents(state.range(0));
        
        // Measure memory allocation overhead of new approach
        auto serialized_bytes = serializer.Encode(events, sequence_number++);
        if (serialized_bytes) {
            bool result = transport.SendBytes(serialized_bytes);
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetLabel("Memory-NewAPI");
}
BENCHMARK(BM_Memory_NewAPI)
    ->Range(10, 100)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// RECEIVING BENCHMARKS
// ============================================================================

// Note: Receiving benchmarks would require a more complex setup with
// producer/consumer threads. For now, focus on sending performance
// which is the main concern for the refactoring.

BENCHMARK_MAIN();