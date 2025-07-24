#pragma once

#include "ITransport.hpp"
#include "EventDataBatch.hpp"
#include <zmq.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <string>

namespace delila {
namespace test {

class ZmqTransport : public ITransport {
public:
    ZmqTransport(ComponentType component);
    ~ZmqTransport() override;
    
    // ITransport interface
    bool Initialize(const Config& config) override;
    bool Send(const EventDataBatch& batch) override;
    bool Receive(EventDataBatch& batch) override;
    void Shutdown() override;
    std::string GetStats() const override;
    bool IsConnected() const override;
    
private:
    ComponentType componentType;
    
    // ZMQ context and sockets
    std::unique_ptr<zmq::context_t> context;
    std::unique_ptr<zmq::socket_t> socket;
    std::unique_ptr<zmq::socket_t> hubPubSocket; // Additional socket for hub's PUB functionality
    
    // Connection state
    std::atomic<bool> isInitialized;
    std::atomic<bool> isConnected;
    std::atomic<bool> isShutdown;
    
    // Configuration
    std::string address;
    uint16_t port;
    uint32_t highWaterMark;
    uint32_t lingerMs;
    uint32_t rcvBufferSize;
    
    // Socket type based on component
    int GetSocketType() const;
    
    // Statistics
    std::atomic<uint64_t> messagesSent;
    std::atomic<uint64_t> messagesReceived;
    std::atomic<uint64_t> bytesSent;
    std::atomic<uint64_t> bytesReceived;
    
    // Component-specific initialization
    bool InitializeAsSender(const Config& config);
    bool InitializeAsReceiver(const Config& config);
    bool InitializeAsHub(const Config& config);
    
    // Helper methods
    std::string BuildAddress(const std::string& host, uint16_t port) const;
    bool SetSocketOptions();
    bool SendMessage(const std::vector<uint8_t>& data);
    bool ReceiveMessage(std::vector<uint8_t>& data);
    
    // ZMQ envelope for PUB/SUB
    struct ZmqEnvelope {
        char topic[32];
        uint32_t size;
    };
    
    static constexpr const char* DATA_TOPIC = "DATA";
};

}} // namespace delila::test