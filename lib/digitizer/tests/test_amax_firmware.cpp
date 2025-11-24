/**
 * @file test_amax_firmware.cpp
 * @brief Unit tests for AMax firmware type and factory support
 *
 * Tests firmware type recognition, factory parsing, and decoder creation
 * Following TDD approach - tests written before implementation
 */

#include <gtest/gtest.h>
#include "IDigitizer.hpp"
#include "DigitizerFactory.hpp"
#include "AMaxDecoder.hpp"

using namespace DELILA::Digitizer;

/**
 * @brief Test fixture for AMax firmware tests
 */
class AMaxFirmwareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// ============================================================================
// Test 1.1: Firmware Type Recognition
// ============================================================================

TEST_F(AMaxFirmwareTest, FirmwareTypeExists) {
    // RED: This test should fail initially until enum is added
    // Verify that AMAX firmware type exists and is distinct from UNKNOWN

    auto amaxType = static_cast<int>(FirmwareType::AMAX);
    auto unknownType = static_cast<int>(FirmwareType::UNKNOWN);

    EXPECT_NE(amaxType, unknownType)
        << "AMAX firmware type should be distinct from UNKNOWN";
}

TEST_F(AMaxFirmwareTest, FirmwareTypeHasCorrectValue) {
    // Verify AMAX has a unique enum value
    // It should be different from all other firmware types

    auto amax = FirmwareType::AMAX;

    EXPECT_NE(amax, FirmwareType::PSD1);
    EXPECT_NE(amax, FirmwareType::PSD2);
    EXPECT_NE(amax, FirmwareType::PHA1);
    EXPECT_NE(amax, FirmwareType::PHA2);
    EXPECT_NE(amax, FirmwareType::QDC1);
    EXPECT_NE(amax, FirmwareType::SCOPE1);
    EXPECT_NE(amax, FirmwareType::SCOPE2);
    EXPECT_NE(amax, FirmwareType::UNKNOWN);
}

// ============================================================================
// Test 1.2: Factory Firmware Type Parsing
// ============================================================================

TEST_F(AMaxFirmwareTest, FactoryParsesFirmwareType) {
    // RED: Should fail until factory is updated
    // Test exact match
    auto type = DigitizerFactory::ParseFirmwareType("AMAX");
    EXPECT_EQ(type, FirmwareType::AMAX)
        << "Factory should parse 'AMAX' as FirmwareType::AMAX";
}

TEST_F(AMaxFirmwareTest, FactoryParsesCaseInsensitive) {
    // Test case-insensitive parsing

    // Lowercase
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType("amax"), FirmwareType::AMAX)
        << "Factory should parse 'amax' (lowercase)";

    // Mixed case
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType("AMax"), FirmwareType::AMAX)
        << "Factory should parse 'AMax' (mixed case)";

    // Another mixed case
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType("aMaX"), FirmwareType::AMAX)
        << "Factory should parse 'aMaX' (mixed case)";
}

TEST_F(AMaxFirmwareTest, FactoryReturnsUnknownForInvalid) {
    // Verify that invalid strings still return UNKNOWN
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType("INVALID"), FirmwareType::UNKNOWN);
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType(""), FirmwareType::UNKNOWN);
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType("AMA"), FirmwareType::UNKNOWN);
    EXPECT_EQ(DigitizerFactory::ParseFirmwareType("AMAXX"), FirmwareType::UNKNOWN);
}

// ============================================================================
// Test 1.3: Factory Decoder Creation
// ============================================================================

TEST_F(AMaxFirmwareTest, FactoryCreatesDecoder) {
    // RED: Should fail until AMaxDecoder exists and factory is updated

    auto decoder = DigitizerFactory::CreateDecoder(FirmwareType::AMAX);

    // Should not return null
    ASSERT_NE(decoder, nullptr)
        << "Factory should create a decoder for AMAX firmware";

    // Verify it's the correct type using dynamic_cast
    auto amaxDecoder = dynamic_cast<AMaxDecoder*>(decoder.get());
    EXPECT_NE(amaxDecoder, nullptr)
        << "Created decoder should be of type AMaxDecoder";
}

TEST_F(AMaxFirmwareTest, FactoryCreatesUniqueInstances) {
    // Each call should create a new instance
    auto decoder1 = DigitizerFactory::CreateDecoder(FirmwareType::AMAX);
    auto decoder2 = DigitizerFactory::CreateDecoder(FirmwareType::AMAX);

    ASSERT_NE(decoder1, nullptr);
    ASSERT_NE(decoder2, nullptr);

    // Should be different instances
    EXPECT_NE(decoder1.get(), decoder2.get())
        << "Factory should create unique decoder instances";
}

// ============================================================================
// Test 1.4: Basic Decoder Interface
// ============================================================================

TEST_F(AMaxFirmwareTest, DecoderImplementsInterface) {
    // Verify that AMaxDecoder properly implements IDecoder interface
    auto decoder = DigitizerFactory::CreateDecoder(FirmwareType::AMAX);
    ASSERT_NE(decoder, nullptr);

    // Should be able to cast to IDecoder
    IDecoder* iDecoder = decoder.get();
    EXPECT_NE(iDecoder, nullptr)
        << "AMaxDecoder should implement IDecoder interface";
}

// ============================================================================
// Test 1.5: Firmware Type String Representation
// ============================================================================

TEST_F(AMaxFirmwareTest, FirmwareTypeToString) {
    // Test that we can convert firmware type back to string
    // This might be useful for logging/debugging

    // This test assumes a ToString method exists or will be added
    // Can be skipped if not needed

    // Example (if such method exists):
    // EXPECT_EQ(FirmwareTypeToString(FirmwareType::AMAX), "AMAX");
}