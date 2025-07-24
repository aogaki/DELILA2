#pragma once

#include "Common.hpp"
#include <vector>
#include <memory>

// Forward declare the EventData type from the digitizer library
// In real implementation, this would be: #include "EventData.hpp"
namespace DELILA {
namespace Digitizer {
    struct EventData {
        double timestamp_ns;
        uint32_t energy;
        uint32_t energy_short;
        uint32_t module;
        uint32_t channel;
        uint32_t time_resolution;
        uint64_t flags;
        // No waveform data (size = 0)
    };
}
}

// Forward declare protobuf message
namespace delila {
namespace test {
    class EventBatchProto;
}
}

namespace delila {
namespace test {

// Event data batch container
class EventDataBatch {
public:
    // Constructor
    EventDataBatch();
    EventDataBatch(uint32_t sourceId, uint64_t sequenceNumber);
    
    // Copy constructor and assignment
    EventDataBatch(const EventDataBatch& other);
    EventDataBatch& operator=(const EventDataBatch& other);
    
    // Move constructor and assignment
    EventDataBatch(EventDataBatch&& other) noexcept;
    EventDataBatch& operator=(EventDataBatch&& other) noexcept;
    
    // Destructor
    ~EventDataBatch();
    
    // Data access
    std::vector<DELILA::Digitizer::EventData>& GetEvents() { return events; }
    const std::vector<DELILA::Digitizer::EventData>& GetEvents() const { return events; }
    
    // Metadata access
    uint64_t GetSequenceNumber() const { return sequenceNumber; }
    void SetSequenceNumber(uint64_t seq) { sequenceNumber = seq; }
    
    uint64_t GetTimestamp() const { return timestamp; }
    void SetTimestamp(uint64_t ts) { timestamp = ts; }
    
    uint32_t GetSourceId() const { return sourceId; }
    void SetSourceId(uint32_t id) { sourceId = id; }
    
    // Utility methods
    size_t GetEventCount() const { return events.size(); }
    size_t GetDataSize() const;
    void Clear();
    void Reserve(size_t count);
    
    // Add events
    void AddEvent(const DELILA::Digitizer::EventData& event);
    void AddEvents(const std::vector<DELILA::Digitizer::EventData>& eventList);
    
    // Serialization methods
    bool SerializeToProtobuf(EventBatchProto& proto) const;
    bool DeserializeFromProtobuf(const EventBatchProto& proto);
    
    // Binary serialization (for ZeroMQ)
    bool SerializeToBinary(std::vector<uint8_t>& buffer) const;
    bool DeserializeFromBinary(const std::vector<uint8_t>& buffer);
    
    // Validation
    bool IsValid() const;
    
private:
    std::vector<DELILA::Digitizer::EventData> events;
    uint64_t sequenceNumber;
    uint64_t timestamp;
    uint32_t sourceId;
};

}} // namespace delila::test