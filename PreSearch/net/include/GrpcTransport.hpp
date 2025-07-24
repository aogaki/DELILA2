#pragma once

#include "ITransport.hpp"
#include "EventDataBatch.hpp"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

// Include generated files
#include "proto/messages.grpc.pb.h"

namespace delila {
namespace test {

class GrpcTransport : public ITransport {
public:
    GrpcTransport(ComponentType component);
    ~GrpcTransport() override;
    
    // ITransport interface
    bool Initialize(const Config& config) override;
    bool Send(const EventDataBatch& batch) override;
    bool Receive(EventDataBatch& batch) override;
    void Shutdown() override;
    std::string GetStats() const override;
    bool IsConnected() const override;
    
private:
    ComponentType componentType;
    std::unique_ptr<grpc::Server> server;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<DataStream::Stub> stub;
    
    // Connection state
    std::atomic<bool> isInitialized;
    std::atomic<bool> isConnected;
    std::atomic<bool> isShutdown;
    
    // Configuration
    std::string serverAddress;
    uint16_t serverPort;
    uint32_t maxMessageSize;
    uint32_t keepaliveTimeMs;
    
    // Streaming
    std::unique_ptr<grpc::ClientContext> clientContext;
    std::unique_ptr<grpc::ClientWriter<EventBatchProto>> writer;
    std::unique_ptr<grpc::ClientReader<EventBatchProto>> reader;
    
    // Thread safety
    std::mutex sendMutex;
    std::mutex receiveMutex;
    
    // Statistics
    std::atomic<uint64_t> messagesSent;
    std::atomic<uint64_t> messagesReceived;
    std::atomic<uint64_t> bytesSent;
    std::atomic<uint64_t> bytesReceived;
    
    // Component-specific initialization
    bool InitializeAsSender(const Config& config);
    bool InitializeAsReceiver(const Config& config);
    bool InitializeAsHub(const Config& config);
    
    // gRPC service implementation
    class DataStreamServiceImpl;
    std::unique_ptr<DataStreamServiceImpl> serviceImpl;
    
    // Helper methods
    grpc::ChannelArguments CreateChannelArguments() const;
    std::string BuildServerAddress(const std::string& host, uint16_t port) const;
    void OnBatchReceived(const EventDataBatch& batch);
};

}} // namespace delila::test