// Test helper utilities for ZMQTransport byte-based testing
#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <memory>
#include <vector>
#include <random>
#include <chrono>

namespace TestHelpers {

// Generate random byte data
inline std::unique_ptr<std::vector<uint8_t>> GenerateRandomBytes(size_t size) {
    auto data = std::make_unique<std::vector<uint8_t>>(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        (*data)[i] = static_cast<uint8_t>(dis(gen));
    }
    
    return data;
}

// Generate pattern data for verification
inline std::unique_ptr<std::vector<uint8_t>> GeneratePatternBytes(size_t size, uint8_t pattern = 0xAB) {
    auto data = std::make_unique<std::vector<uint8_t>>(size);
    
    for (size_t i = 0; i < size; ++i) {
        // Create a recognizable pattern
        (*data)[i] = static_cast<uint8_t>((pattern + i) % 256);
    }
    
    return data;
}

// Verify pattern data
inline bool VerifyPatternBytes(const std::vector<uint8_t>& data, uint8_t pattern = 0xAB) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != static_cast<uint8_t>((pattern + i) % 256)) {
            return false;
        }
    }
    return true;
}

// Compare two byte vectors
inline bool CompareBytes(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

// Create a specific test pattern
enum class TestPattern {
    ZEROS,
    ONES,
    ALTERNATING,
    SEQUENTIAL,
    RANDOM
};

inline std::unique_ptr<std::vector<uint8_t>> CreateTestPattern(size_t size, TestPattern pattern) {
    auto data = std::make_unique<std::vector<uint8_t>>(size);
    
    switch (pattern) {
        case TestPattern::ZEROS:
            std::fill(data->begin(), data->end(), 0x00);
            break;
            
        case TestPattern::ONES:
            std::fill(data->begin(), data->end(), 0xFF);
            break;
            
        case TestPattern::ALTERNATING:
            for (size_t i = 0; i < size; ++i) {
                (*data)[i] = (i % 2) ? 0xFF : 0x00;
            }
            break;
            
        case TestPattern::SEQUENTIAL:
            for (size_t i = 0; i < size; ++i) {
                (*data)[i] = static_cast<uint8_t>(i % 256);
            }
            break;
            
        case TestPattern::RANDOM:
            return GenerateRandomBytes(size);
    }
    
    return data;
}

// Measure time for performance testing
class Timer {
public:
    Timer() : start(std::chrono::high_resolution_clock::now()) {}
    
    double ElapsedMs() const {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> diff = end - start;
        return diff.count();
    }
    
    void Reset() {
        start = std::chrono::high_resolution_clock::now();
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

// Mock data that simulates serialized EventData
inline std::unique_ptr<std::vector<uint8_t>> CreateMockSerializedEventData(
    uint32_t event_count = 10,
    uint32_t event_size = 100) {
    
    // Simple format: [header][event1][event2]...
    // Header: magic(4) + event_count(4) + event_size(4) = 12 bytes
    size_t total_size = 12 + (event_count * event_size);
    auto data = std::make_unique<std::vector<uint8_t>>(total_size);
    
    // Write header
    uint32_t magic = 0xDEADBEEF;
    std::memcpy(data->data(), &magic, 4);
    std::memcpy(data->data() + 4, &event_count, 4);
    std::memcpy(data->data() + 8, &event_size, 4);
    
    // Write mock events
    for (uint32_t i = 0; i < event_count; ++i) {
        size_t offset = 12 + (i * event_size);
        // Fill with pattern based on event index
        for (uint32_t j = 0; j < event_size; ++j) {
            (*data)[offset + j] = static_cast<uint8_t>((i + j) % 256);
        }
    }
    
    return data;
}

// Verify mock serialized data
inline bool VerifyMockSerializedEventData(const std::vector<uint8_t>& data) {
    if (data.size() < 12) return false;
    
    // Check magic number
    uint32_t magic;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0xDEADBEEF) return false;
    
    // Get counts
    uint32_t event_count, event_size;
    std::memcpy(&event_count, data.data() + 4, 4);
    std::memcpy(&event_size, data.data() + 8, 4);
    
    // Verify size
    size_t expected_size = 12 + (event_count * event_size);
    if (data.size() != expected_size) return false;
    
    // Verify event patterns
    for (uint32_t i = 0; i < event_count; ++i) {
        size_t offset = 12 + (i * event_size);
        for (uint32_t j = 0; j < event_size; ++j) {
            if (data[offset + j] != static_cast<uint8_t>((i + j) % 256)) {
                return false;
            }
        }
    }
    
    return true;
}

} // namespace TestHelpers

#endif // TEST_HELPERS_HPP