#include <iostream>
#include <cstdint>
#include <cstddef>

struct __attribute__((packed)) SerializedEventDataHeader {
    uint8_t analogProbe1Type;   // 1 byte - offset 0
    uint8_t analogProbe2Type;   // 1 byte - offset 1  
    uint8_t channel;            // 1 byte - offset 2
    uint8_t digitalProbe1Type;  // 1 byte - offset 3
    uint8_t digitalProbe2Type;  // 1 byte - offset 4
    uint8_t digitalProbe3Type;  // 1 byte - offset 5
    uint8_t digitalProbe4Type;  // 1 byte - offset 6
    uint8_t downSampleFactor;   // 1 byte - offset 7
    uint16_t energy;            // 2 bytes - offset 8
    uint16_t energyShort;       // 2 bytes - offset 10
    uint64_t flags;             // 8 bytes - offset 12
    uint8_t module;             // 1 byte - offset 20
    uint8_t timeResolution;     // 1 byte - offset 21
    double timeStampNs;         // 8 bytes - offset 22
    uint32_t waveformSize;      // 4 bytes - offset 30
}; // Expected: 34 bytes

int main() {
    std::cout << "SerializedEventDataHeader size: " << sizeof(SerializedEventDataHeader) << std::endl;
    std::cout << "Expected size: 1+1+1+1+1+1+1+1+2+2+8+1+1+8+4 = " << (1+1+1+1+1+1+1+1+2+2+8+1+1+8+4) << std::endl;
    
    std::cout << "Field offsets:" << std::endl;
    std::cout << "analogProbe1Type: " << offsetof(SerializedEventDataHeader, analogProbe1Type) << std::endl;
    std::cout << "analogProbe2Type: " << offsetof(SerializedEventDataHeader, analogProbe2Type) << std::endl;
    std::cout << "channel: " << offsetof(SerializedEventDataHeader, channel) << std::endl;
    std::cout << "digitalProbe1Type: " << offsetof(SerializedEventDataHeader, digitalProbe1Type) << std::endl;
    std::cout << "digitalProbe2Type: " << offsetof(SerializedEventDataHeader, digitalProbe2Type) << std::endl;
    std::cout << "digitalProbe3Type: " << offsetof(SerializedEventDataHeader, digitalProbe3Type) << std::endl;
    std::cout << "digitalProbe4Type: " << offsetof(SerializedEventDataHeader, digitalProbe4Type) << std::endl;
    std::cout << "downSampleFactor: " << offsetof(SerializedEventDataHeader, downSampleFactor) << std::endl;
    std::cout << "energy: " << offsetof(SerializedEventDataHeader, energy) << std::endl;
    std::cout << "energyShort: " << offsetof(SerializedEventDataHeader, energyShort) << std::endl;
    std::cout << "flags: " << offsetof(SerializedEventDataHeader, flags) << std::endl;
    std::cout << "module: " << offsetof(SerializedEventDataHeader, module) << std::endl;
    std::cout << "timeResolution: " << offsetof(SerializedEventDataHeader, timeResolution) << std::endl;
    std::cout << "timeStampNs: " << offsetof(SerializedEventDataHeader, timeStampNs) << std::endl;
    std::cout << "waveformSize: " << offsetof(SerializedEventDataHeader, waveformSize) << std::endl;
    
    return 0;
}