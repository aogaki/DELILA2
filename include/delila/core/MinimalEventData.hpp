#ifndef DELILA_CORE_MINIMALEVENTDATA_HPP
#define DELILA_CORE_MINIMALEVENTDATA_HPP

#include <cstdint>

namespace DELILA {
namespace Digitizer {

class MinimalEventData {
public:
    // Default constructor - zero-initialize all fields
    MinimalEventData() 
        : module(0)
        , channel(0) 
        , energy(0)
        , energyShort(0)
        , timeStampNs(0.0)
        , flags(0)
    {
    }

    // Parameterized constructor
    MinimalEventData(uint8_t mod, uint8_t ch, double timestamp, uint16_t en, uint16_t enShort, uint64_t fl)
        : module(mod)
        , channel(ch)
        , energy(en)
        , energyShort(enShort)
        , timeStampNs(timestamp)
        , flags(fl)
    {
    }

    // Flag bit definitions (compatible with EventData)
    static constexpr uint64_t FLAG_PILEUP = 0x01;          // Pileup detected
    static constexpr uint64_t FLAG_TRIGGER_LOST = 0x02;    // Trigger lost
    static constexpr uint64_t FLAG_OVER_RANGE = 0x04;      // Signal saturation

    // Flag helper methods
    bool HasPileup() const { return (flags & FLAG_PILEUP) != 0; }
    bool HasTriggerLost() const { return (flags & FLAG_TRIGGER_LOST) != 0; }
    bool HasOverRange() const { return (flags & FLAG_OVER_RANGE) != 0; }

    // Public data members - ordered for optimal packing
    uint8_t module;          // 1 byte - Hardware module ID
    uint8_t channel;         // 1 byte - Channel within module
    uint16_t energy;         // 2 bytes - Primary energy measurement
    uint16_t energyShort;    // 2 bytes - Short gate energy
    // 2 bytes padding here for double alignment
    double timeStampNs;      // 8 bytes - Timestamp in nanoseconds
    uint64_t flags;          // 8 bytes - Status/error flags
} __attribute__((packed));

static_assert(sizeof(MinimalEventData) == 22, "MinimalEventData packed size is 22 bytes");

} // namespace Digitizer
} // namespace DELILA

#endif // DELILA_CORE_MINIMALEVENTDATA_HPP