#include "EventDataBatch.hpp"
#include "messages.pb.h"
#include <cstring>

namespace delila {
namespace test {

EventDataBatch::EventDataBatch() 
    : sequenceNumber(0), timestamp(0), sourceId(0) {
}

EventDataBatch::EventDataBatch(uint32_t sourceId, uint64_t sequenceNumber)
    : sequenceNumber(sequenceNumber), timestamp(GetCurrentTimestampNs()), sourceId(sourceId) {
}

EventDataBatch::EventDataBatch(const EventDataBatch& other)
    : events(other.events), sequenceNumber(other.sequenceNumber), 
      timestamp(other.timestamp), sourceId(other.sourceId) {
}

EventDataBatch& EventDataBatch::operator=(const EventDataBatch& other) {
    if (this != &other) {
        events = other.events;
        sequenceNumber = other.sequenceNumber;
        timestamp = other.timestamp;
        sourceId = other.sourceId;
    }
    return *this;
}

EventDataBatch::EventDataBatch(EventDataBatch&& other) noexcept
    : events(std::move(other.events)), sequenceNumber(other.sequenceNumber),
      timestamp(other.timestamp), sourceId(other.sourceId) {
    other.sequenceNumber = 0;
    other.timestamp = 0;
    other.sourceId = 0;
}

EventDataBatch& EventDataBatch::operator=(EventDataBatch&& other) noexcept {
    if (this != &other) {
        events = std::move(other.events);
        sequenceNumber = other.sequenceNumber;
        timestamp = other.timestamp;
        sourceId = other.sourceId;
        
        other.sequenceNumber = 0;
        other.timestamp = 0;
        other.sourceId = 0;
    }
    return *this;
}

EventDataBatch::~EventDataBatch() = default;

size_t EventDataBatch::GetDataSize() const {
    return events.size() * sizeof(DELILA::Digitizer::EventData);
}

void EventDataBatch::Clear() {
    events.clear();
    sequenceNumber = 0;
    timestamp = 0;
    sourceId = 0;
}

void EventDataBatch::Reserve(size_t count) {
    events.reserve(count);
}

void EventDataBatch::AddEvent(const DELILA::Digitizer::EventData& event) {
    events.push_back(event);
}

void EventDataBatch::AddEvents(const std::vector<DELILA::Digitizer::EventData>& eventList) {
    events.insert(events.end(), eventList.begin(), eventList.end());
}

bool EventDataBatch::SerializeToProtobuf(EventBatchProto& proto) const {
    try {
        proto.Clear();
        
        // Set metadata
        proto.set_sequence_number(sequenceNumber);
        proto.set_timestamp(timestamp);
        proto.set_source_id(sourceId);
        
        // Add events
        for (const auto& event : events) {
            auto* eventProto = proto.add_events();
            eventProto->set_timestamp_ns(event.timestamp_ns);
            eventProto->set_energy(event.energy);
            eventProto->set_energy_short(event.energy_short);
            eventProto->set_module(event.module);
            eventProto->set_channel(event.channel);
            eventProto->set_time_resolution(event.time_resolution);
            eventProto->set_flags(event.flags);
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool EventDataBatch::DeserializeFromProtobuf(const EventBatchProto& proto) {
    try {
        Clear();
        
        // Get metadata
        sequenceNumber = proto.sequence_number();
        timestamp = proto.timestamp();
        sourceId = proto.source_id();
        
        // Get events
        events.reserve(proto.events_size());
        for (int i = 0; i < proto.events_size(); ++i) {
            const auto& eventProto = proto.events(i);
            DELILA::Digitizer::EventData event;
            event.timestamp_ns = eventProto.timestamp_ns();
            event.energy = eventProto.energy();
            event.energy_short = eventProto.energy_short();
            event.module = eventProto.module();
            event.channel = eventProto.channel();
            event.time_resolution = eventProto.time_resolution();
            event.flags = eventProto.flags();
            events.push_back(event);
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool EventDataBatch::SerializeToBinary(std::vector<uint8_t>& buffer) const {
    try {
        EventBatchProto proto;
        if (!SerializeToProtobuf(proto)) {
            return false;
        }
        
        size_t size = proto.ByteSizeLong();
        buffer.resize(size);
        return proto.SerializeToArray(buffer.data(), size);
    } catch (const std::exception&) {
        return false;
    }
}

bool EventDataBatch::DeserializeFromBinary(const std::vector<uint8_t>& buffer) {
    try {
        EventBatchProto proto;
        if (!proto.ParseFromArray(buffer.data(), buffer.size())) {
            return false;
        }
        
        return DeserializeFromProtobuf(proto);
    } catch (const std::exception&) {
        return false;
    }
}

bool EventDataBatch::IsValid() const {
    return !events.empty() && sourceId != 0;
}

}} // namespace delila::test