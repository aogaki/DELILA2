#include <iostream>
#include <iomanip>
#include "PSD1Constants.hpp"

using namespace DELILA::Digitizer;

void testExtraOptionDecoding() {
    std::cout << "Testing PSD1 Extra Option Timestamp Decoding" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Test data
    uint32_t extrasWord = 0x12345678;  // Example extras word
    uint32_t triggerTimeTag = 0x0ABCDEF0;  // Example trigger time tag
    uint32_t timeStep = 2;  // 2ns time step
    
    std::cout << "Test Data:" << std::endl;
    std::cout << "  Extras Word: 0x" << std::hex << extrasWord << std::dec << std::endl;
    std::cout << "  Trigger Time Tag: 0x" << std::hex << triggerTimeTag << std::dec << std::endl;
    std::cout << "  Time Step: " << timeStep << " ns" << std::endl;
    std::cout << std::endl;
    
    // Test different extra options
    for (uint8_t extraOption = 0; extraOption <= 2; ++extraOption) {
        std::cout << "Extra Option " << static_cast<int>(extraOption) << " (0b" 
                  << std::bitset<3>(extraOption) << "):" << std::endl;
        
        uint16_t extendedTime = 0;
        uint16_t fineTimeStamp = 0;
        
        switch (extraOption) {
            case PSD1Constants::ExtraFormats::kExtendedTimestampOnly:
            case PSD1Constants::ExtraFormats::kExtendedTimestampOnly1:
                extendedTime = (extrasWord >> PSD1Constants::Event::kExtendedTimeShift) & 
                              PSD1Constants::Event::kExtendedTimeMask;
                std::cout << "  Extended Time: " << extendedTime << " (0x" << std::hex << extendedTime << std::dec << ")" << std::endl;
                std::cout << "  Fine Time: Not available" << std::endl;
                std::cout << "  Event Flags: Not available" << std::endl;
                break;
                
            case PSD1Constants::ExtraFormats::kExtendedFlagsFineTT:
                fineTimeStamp = extrasWord & PSD1Constants::Event::kFineTimeStampMask;
                uint8_t flags = (extrasWord >> PSD1Constants::Event::kFlagsShift) & PSD1Constants::Event::kFlagsMask;
                extendedTime = (extrasWord >> PSD1Constants::Event::kExtendedTimeShift) & 
                              PSD1Constants::Event::kExtendedTimeMask;
                
                std::cout << "  Extended Time: " << extendedTime << " (0x" << std::hex << extendedTime << std::dec << ")" << std::endl;
                std::cout << "  Fine Time: " << fineTimeStamp << " (0x" << std::hex << fineTimeStamp << std::dec << ")" << std::endl;
                std::cout << "  Event Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec << std::endl;
                break;
        }
        
        // Calculate final timestamp
        uint64_t extendedTimestamp = static_cast<uint64_t>(extendedTime) << 31;
        uint64_t combinedTimeTag = static_cast<uint64_t>(triggerTimeTag) + extendedTimestamp;
        uint64_t finalTimestamp = combinedTimeTag * timeStep;
        
        double fineTimeNs = 0.0;
        if (extraOption == PSD1Constants::ExtraFormats::kExtendedFlagsFineTT) {
            fineTimeNs = (static_cast<double>(fineTimeStamp) / 1024.0) * timeStep;
        }
        
        double finalTimeStampNs = static_cast<double>(finalTimestamp) + fineTimeNs;
        
        std::cout << "  Final Timestamp: " << std::fixed << std::setprecision(3) 
                  << finalTimeStampNs << " ns" << std::endl;
        std::cout << std::endl;
    }
}

int main() {
    testExtraOptionDecoding();
    return 0;
}