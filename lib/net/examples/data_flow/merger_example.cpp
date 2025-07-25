/**
 * @file merger_example.cpp
 * @brief Example of a merger module collecting data from multiple fetchers
 * 
 * Demonstrates:
 * - PULL socket for work collection from multiple fetchers
 * - PUB socket for broadcasting sorted data to sinks
 * - Binary data deserialization and re-serialization
 * - Timestamp-based event sorting
 */

#include <delila/net/delila_net.hpp>
#include <delila/digitizer/EventData.hpp>
#include <iostream>
#include <queue>
#include <thread>
#include <algorithm>

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

// Priority queue for timestamp-based sorting
struct EventComparator {
    bool operator()(const EventData& a, const EventData& b) {
        return a.timeStampNs > b.timeStampNs;  // Min-heap (earliest timestamp first)
    }
};

int main() {
    try {
        // Create connections
        Connection data_puller(Connection::PULLER);
        Connection data_publisher(Connection::PUBLISHER);
        
        // Bind to receive data from multiple fetchers
        auto pull_result = data_puller.bind("*:5555", Connection::AUTO);
        if (!isOk(pull_result)) {
            std::cerr << "Failed to bind puller: " << getError(pull_result).message << std::endl;
            return 1;
        }
        
        // Bind to publish sorted data to sinks
        auto pub_result = data_publisher.bind("*:5556", Connection::AUTO);
        if (!isOk(pub_result)) {
            std::cerr << "Failed to bind publisher: " << getError(pub_result).message << std::endl;
            return 1;
        }
        
        // Create serializer and sequence tracker
        BinarySerializer serializer;
        SequenceTracker sequence_tracker;
        
        // Event sorting buffer
        std::priority_queue<EventData, std::vector<EventData>, EventComparator> event_queue;
        const size_t BATCH_SIZE = 100;  // Send batches of 100 events
        
        std::cout << "Merger started - collecting from fetchers, publishing to sinks..." << std::endl;
        
        int batches_processed = 0;
        
        while (batches_processed < 200) {  // Process ~200 batches
            // Receive data from fetchers
            auto receive_result = data_puller.receiveRaw();
            if (isOk(receive_result)) {
                const auto& raw_data = getValue(receive_result);
                
                // Decode events
                auto decode_result = serializer.decode(raw_data);
                if (isOk(decode_result)) {
                    const auto& events = getValue(decode_result);
                    
                    // Check sequence number for gaps
                    const auto* header = reinterpret_cast<const BinaryDataHeader*>(raw_data.data());
                    auto seq_result = sequence_tracker.checkSequence(header->sequence_number);
                    if (!isOk(seq_result)) {
                        std::cout << "Sequence gap detected: " << getError(seq_result).message << std::endl;
                    }
                    
                    // Add events to sorting queue
                    for (const auto& event : events) {
                        event_queue.push(event);
                    }
                    
                    std::cout << "Received " << events.size() << " events, queue size: " 
                              << event_queue.size() << std::endl;
                    
                    // Send sorted batches when we have enough events
                    while (event_queue.size() >= BATCH_SIZE) {
                        std::vector<EventData> sorted_batch;
                        sorted_batch.reserve(BATCH_SIZE);
                        
                        // Extract earliest events
                        for (size_t i = 0; i < BATCH_SIZE && !event_queue.empty(); ++i) {
                            sorted_batch.push_back(event_queue.top());
                            event_queue.pop();
                        }
                        
                        // Serialize and publish sorted batch
                        auto encode_result = serializer.encode(sorted_batch);
                        if (isOk(encode_result)) {
                            auto send_result = data_publisher.sendRaw(getValue(encode_result));
                            if (isOk(send_result)) {
                                batches_processed++;
                                std::cout << "Published sorted batch " << batches_processed 
                                          << " with " << sorted_batch.size() << " events" << std::endl;
                            } else {
                                std::cerr << "Publish failed: " << getError(send_result).message << std::endl;
                            }
                        } else {
                            std::cerr << "Encoding failed: " << getError(encode_result).message << std::endl;
                        }
                    }
                    
                } else {
                    std::cerr << "Decoding failed: " << getError(decode_result).message << std::endl;
                }
            } else {
                std::cerr << "Receive failed: " << getError(receive_result).message << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        // Flush remaining events
        if (!event_queue.empty()) {
            std::vector<EventData> final_batch;
            while (!event_queue.empty()) {
                final_batch.push_back(event_queue.top());
                event_queue.pop();
            }
            
            auto encode_result = serializer.encode(final_batch);
            if (isOk(encode_result)) {
                data_publisher.sendRaw(getValue(encode_result));
                std::cout << "Published final batch with " << final_batch.size() << " events" << std::endl;
            }
        }
        
        std::cout << "Merger finished" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}