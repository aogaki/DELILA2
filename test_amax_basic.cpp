/**
 * @file test_amax_basic.cpp
 * @brief Basic test for AMax firmware support
 */

#include <iostream>
#include <memory>
#include <cassert>

#include "lib/digitizer/include/IDigitizer.hpp"
#include "lib/digitizer/include/AMaxDecoder.hpp"
#include "lib/digitizer/include/AMaxConstants.hpp"

using namespace DELILA::Digitizer;

// Test firmware type enum
void testFirmwareType() {
    std::cout << "Testing AMax firmware type..." << std::endl;

    // Test that AMAX enum exists
    FirmwareType type = FirmwareType::AMAX;

    // Test that it's distinct from other types
    assert(type != FirmwareType::PSD1);
    assert(type != FirmwareType::PSD2);
    assert(type != FirmwareType::PHA1);
    assert(type != FirmwareType::UNKNOWN);

    std::cout << "  ✓ AMax firmware type enum exists and is unique" << std::endl;
}

// Test decoder creation
void testDecoderCreation() {
    std::cout << "Testing AMaxDecoder creation..." << std::endl;

    // Create decoder
    auto decoder = std::make_unique<AMaxDecoder>();
    assert(decoder != nullptr);

    // Test configuration methods
    decoder->SetTimeStep(2);
    decoder->SetDumpFlag(true);
    decoder->SetModuleNumber(3);

    std::cout << "  ✓ AMaxDecoder created and configured" << std::endl;
}

// Test data processing (stub)
void testDataProcessing() {
    std::cout << "Testing AMaxDecoder data processing..." << std::endl;

    auto decoder = std::make_unique<AMaxDecoder>();
    decoder->SetDumpFlag(true);

    // Create test data
    auto rawData = std::make_unique<RawData_t>();
    rawData->size = 256;
    rawData->data.resize(256);
    rawData->nEvents = 0;

    // Fill with test pattern
    for (size_t i = 0; i < rawData->data.size(); i++) {
        rawData->data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Process data
    DataType dataType = decoder->AddData(std::move(rawData));
    assert(dataType == DataType::Event);

    // Get events (should be empty in stub)
    auto events = decoder->GetEventData();
    assert(events != nullptr);
    assert(events->empty());  // Stub returns empty vector

    std::cout << "  ✓ AMaxDecoder processes data without crash" << std::endl;
}

// Test register mapping
void testRegisterMapping() {
    std::cout << "Testing AMax register constants..." << std::endl;

    // Test some key register addresses
    assert(AMaxConstants::REG_HOOOLD == 0x0000);
    assert(AMaxConstants::REG_GAIN == 0x0007);
    assert(AMaxConstants::REG_REG_EN == 0xE0020);

    // Test register name lookup
    uint32_t addr = AMaxConstants::GetRegisterAddress("hooold");
    assert(addr == 0x0000);

    addr = AMaxConstants::GetRegisterAddress("GAIN");
    assert(addr == 0x0007);

    // Test that unknown register throws
    bool exceptionThrown = false;
    try {
        AMaxConstants::GetRegisterAddress("unknown_register");
    } catch (const std::runtime_error&) {
        exceptionThrown = true;
    }
    assert(exceptionThrown);

    std::cout << "  ✓ Register mapping works correctly" << std::endl;
}

int main() {
    std::cout << "\n=== AMax Basic Functionality Tests ===" << std::endl;

    try {
        testFirmwareType();
        testDecoderCreation();
        testDataProcessing();
        testRegisterMapping();

        std::cout << "\n✅ All tests passed!\n" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}