/**
 * @file data_fetcher_example.cpp
 * @brief Example of a data fetcher module sending binary data to merger
 * 
 * Demonstrates:
 * - PUSH socket for work distribution
 * - Binary data serialization with EventData
 * - Automatic transport selection (IPC for localhost)
 */

#include <delila/net/delila_net.hpp>
#include <delila/digitizer/EventData.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    try {
        // Create connection for sending data to merger
        Connection data_pusher(Connection::PUSHER);
        
        // Connect to merger (auto-selects IPC for localhost)
        auto result = data_pusher.connect("127.0.0.1:5555", Connection::AUTO);
        if (!isOk(result)) {
            std::cerr << "Failed to connect: " << getError(result).message << std::endl;
            return 1;
        }
        
        // Create serializer with compression
        BinarySerializer serializer;
        
        std::cout << "Data Fetcher started - sending data to merger..." << std::endl;
        
        // Simulate data acquisition loop
        for (int batch = 0; batch < 100; ++batch) {
            std::vector<EventData> events;
            
            // Generate sample events (simulating digitizer data)
            for (int i = 0; i < 50; ++i) {
                EventData event;
                event.timeStampNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                event.energy = 1000 + (batch * 50 + i) % 4000;  // 1-5 keV range
                event.energyShort = event.energy / 2;
                event.module = 1;  // Module ID
                event.channel = i % 16;  // 16 channels per module
                event.flags = 0;  // No error flags
                
                // Add some waveform data occasionally
                if (i % 10 == 0) {
                    event.ResizeWaveform(1000);  // 1000 samples
                    for (size_t j = 0; j < 1000; ++j) {
                        event.analogProbe1[j] = 2048 + static_cast<int32_t>(100 * std::sin(j * 0.1));
                        event.analogProbe2[j] = 2048 - static_cast<int32_t>(50 * std::cos(j * 0.1));
                    }
                }
                
                events.push_back(std::move(event));
            }
            
            // Serialize and send batch
            auto encode_result = serializer.encode(events);
            if (isOk(encode_result)) {
                auto send_result = data_pusher.sendRaw(getValue(encode_result));
                if (isOk(send_result)) {
                    std::cout << "Sent batch " << batch << " with " << events.size() 
                              << " events (" << getValue(encode_result).size() << " bytes)" << std::endl;
                } else {
                    std::cerr << "Send failed: " << getError(send_result).message << std::endl;
                }
            } else {
                std::cerr << "Encoding failed: " << getError(encode_result).message << std::endl;
            }
            
            // Simulate acquisition rate (20 Hz)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::cout << "Data Fetcher finished" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}