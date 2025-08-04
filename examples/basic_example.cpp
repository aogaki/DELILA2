/**
 * @file basic_example.cpp
 * @brief Basic example demonstrating DELILA2 Digitizer instantiation
 * 
 * This example shows how to:
 * - Include the DELILA2 library
 * - Create a Digitizer instance
 * - Initialize and configure the digitizer
 * - Handle the case when no hardware is connected
 */

#include <iostream>
#include <memory>
#include <delila/delila.hpp>

using namespace DELILA;

int main(int argc, char* argv[]) {
    std::cout << "DELILA2 Basic Digitizer Example" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Initialize DELILA2 library
    std::cout << "\n1. Initializing DELILA2 library..." << std::endl;
    if (DELILA::initialize()) {
        std::cout << "   ✓ DELILA2 v" << DELILA::getVersion() << " initialized successfully" << std::endl;
    } else {
        std::cerr << "   ✗ Failed to initialize DELILA2" << std::endl;
        return 1;
    }
    
    // Create Digitizer instance
    std::cout << "\n2. Creating Digitizer instance..." << std::endl;
    try {
        Digitizer::Digitizer digitizer;
        std::cout << "   ✓ Digitizer instance created successfully" << std::endl;
        
        // Note: Without actual hardware or configuration, we can't do much more
        // In a real application, you would:
        // 1. Create a ConfigurationManager with proper settings
        // 2. Initialize the digitizer with the configuration
        // 3. Start data acquisition
        // 4. Read events in a loop
        
        std::cout << "\n3. Digitizer Configuration (Simulation)" << std::endl;
        std::cout << "   Note: Without hardware connected, actual initialization will fail." << std::endl;
        std::cout << "   In production code, you would:" << std::endl;
        std::cout << "   - Load configuration from file" << std::endl;
        std::cout << "   - Call digitizer.Initialize(config)" << std::endl;
        std::cout << "   - Call digitizer.Configure()" << std::endl;
        std::cout << "   - Call digitizer.StartAcquisition()" << std::endl;
        std::cout << "   - Read events with digitizer.GetEventData()" << std::endl;
        
        // Example of what production code might look like (commented out):
        /*
        // Load configuration
        Digitizer::ConfigurationManager config;
        if (config.LoadFromFile("digitizer.conf") == Digitizer::ConfigurationManager::LoadResult::Success) {
            
            // Initialize digitizer
            if (digitizer.Initialize(config)) {
                std::cout << "   ✓ Digitizer initialized" << std::endl;
                
                // Configure digitizer
                if (digitizer.Configure()) {
                    std::cout << "   ✓ Digitizer configured" << std::endl;
                    
                    // Print device info
                    digitizer.PrintDeviceInfo();
                    
                    // Start acquisition
                    if (digitizer.StartAcquisition()) {
                        std::cout << "   ✓ Acquisition started" << std::endl;
                        
                        // Read some events
                        for (int i = 0; i < 10; ++i) {
                            auto events = digitizer.GetEventData();
                            if (events && !events->empty()) {
                                std::cout << "   Got " << events->size() << " events" << std::endl;
                            }
                        }
                        
                        // Stop acquisition
                        digitizer.StopAcquisition();
                        std::cout << "   ✓ Acquisition stopped" << std::endl;
                    }
                }
            }
        }
        */
        
    } catch (const std::exception& e) {
        std::cerr << "   ✗ Exception: " << e.what() << std::endl;
    }
    
    // Demonstrate EventData structure
    std::cout << "\n4. Creating EventData instance..." << std::endl;
    {
        Digitizer::EventData event;
        event.module = 1;
        event.channel = 0;
        event.timeStampNs = 1234567890;
        event.energy = 2500;
        event.energyShort = 2400;
        
        std::cout << "   ✓ EventData created:" << std::endl;
        std::cout << "     - Module: " << static_cast<int>(event.module) << std::endl;
        std::cout << "     - Channel: " << static_cast<int>(event.channel) << std::endl;
        std::cout << "     - Timestamp: " << event.timeStampNs << " ns" << std::endl;
        std::cout << "     - Energy: " << event.energy << std::endl;
        std::cout << "     - Energy (short): " << event.energyShort << std::endl;
    }
    
    // Demonstrate DataType enum
    std::cout << "\n5. Using DataType enum..." << std::endl;
    {
        auto eventType = Digitizer::DataType::Event;
        auto startType = Digitizer::DataType::Start;
        auto stopType = Digitizer::DataType::Stop;
        
        std::cout << "   ✓ DataType enum values:" << std::endl;
        std::cout << "     - Event: " << static_cast<int>(eventType) << std::endl;
        std::cout << "     - Start: " << static_cast<int>(startType) << std::endl;
        std::cout << "     - Stop: " << static_cast<int>(stopType) << std::endl;
    }
    
    // Shutdown
    std::cout << "\n6. Shutting down DELILA2..." << std::endl;
    DELILA::shutdown();
    std::cout << "   ✓ DELILA2 shutdown complete" << std::endl;
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}