#include <gtest/gtest.h>
#include "delila/core/MinimalEventData.hpp"

using namespace DELILA::Digitizer;

class MinimalEventDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }
    
    void TearDown() override {
        // Cleanup if needed  
    }
};

TEST_F(MinimalEventDataTest, DefaultConstruction) {
    MinimalEventData event;
    
    // Verify all fields are zero-initialized
    EXPECT_EQ(event.module, 0);
    EXPECT_EQ(event.channel, 0);
    EXPECT_EQ(event.timeStampNs, 0.0);
    EXPECT_EQ(event.energy, 0);
    EXPECT_EQ(event.energyShort, 0);
    EXPECT_EQ(event.flags, 0);
}

TEST_F(MinimalEventDataTest, ParameterizedConstruction) {
    MinimalEventData event(1, 2, 1234.5, 100, 50, 0x01);
    
    // Verify all fields are set correctly
    EXPECT_EQ(event.module, 1);
    EXPECT_EQ(event.channel, 2);
    EXPECT_EQ(event.timeStampNs, 1234.5);
    EXPECT_EQ(event.energy, 100);
    EXPECT_EQ(event.energyShort, 50);
    EXPECT_EQ(event.flags, 0x01);
}

TEST_F(MinimalEventDataTest, SizeVerification) {
    // Verify our packed structure is exactly 22 bytes
    EXPECT_EQ(sizeof(MinimalEventData), 22);
    
    // Verify it's much smaller than a typical EventData
    // (EventData with waveforms would be 100s of bytes)
    EXPECT_LT(sizeof(MinimalEventData), 100);
}

TEST_F(MinimalEventDataTest, CopyConstruction) {
    MinimalEventData original(1, 2, 1234.5, 100, 50, 0x01);
    MinimalEventData copy(original);
    
    // Verify all fields copied correctly
    EXPECT_EQ(copy.module, original.module);
    EXPECT_EQ(copy.channel, original.channel);
    EXPECT_EQ(copy.timeStampNs, original.timeStampNs);
    EXPECT_EQ(copy.energy, original.energy);
    EXPECT_EQ(copy.energyShort, original.energyShort);
    EXPECT_EQ(copy.flags, original.flags);
}

TEST_F(MinimalEventDataTest, CopyAssignment) {
    MinimalEventData original(1, 2, 1234.5, 100, 50, 0x01);
    MinimalEventData copy;
    
    copy = original;
    
    // Verify all fields copied correctly  
    EXPECT_EQ(copy.module, original.module);
    EXPECT_EQ(copy.channel, original.channel);
    EXPECT_EQ(copy.timeStampNs, original.timeStampNs);
    EXPECT_EQ(copy.energy, original.energy);
    EXPECT_EQ(copy.energyShort, original.energyShort);
    EXPECT_EQ(copy.flags, original.flags);
}

TEST_F(MinimalEventDataTest, FlagHelpers) {
    MinimalEventData event;
    
    // Test individual flag setting
    event.flags = MinimalEventData::FLAG_PILEUP;
    EXPECT_TRUE(event.HasPileup());
    EXPECT_FALSE(event.HasTriggerLost());
    EXPECT_FALSE(event.HasOverRange());
    
    // Test multiple flags
    event.flags = MinimalEventData::FLAG_PILEUP | MinimalEventData::FLAG_OVER_RANGE;
    EXPECT_TRUE(event.HasPileup());
    EXPECT_TRUE(event.HasOverRange());
    EXPECT_FALSE(event.HasTriggerLost());
    
    // Test all flags clear
    event.flags = 0;
    EXPECT_FALSE(event.HasPileup());
    EXPECT_FALSE(event.HasTriggerLost());
    EXPECT_FALSE(event.HasOverRange());
}