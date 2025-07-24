#include "GrpcTransport.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "proto/messages.grpc.pb.h"
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <sstream>

namespace delila {
namespace test {

// gRPC service implementation
class GrpcTransport::DataStreamServiceImpl final : public DataStream::Service {
public:
    DataStreamServiceImpl(GrpcTransport* transport) : transport_(transport) {}
    
    grpc::Status SendData(grpc::ServerContext* context,
                         grpc::ServerReader<EventBatchProto>* reader,
                         Acknowledgment* response) override {
        EventBatchProto batch;
        uint64_t lastSequence = 0;
        
        while (reader->Read(&batch)) {
            // Convert to EventDataBatch and forward
            EventDataBatch dataBatch;
            if (dataBatch.DeserializeFromProtobuf(batch)) {
                // Store in receive queue or forward directly
                transport_->OnBatchReceived(dataBatch);
                lastSequence = batch.sequence_number();
            }
        }
        
        response->set_success(true);
        response->set_last_sequence(lastSequence);
        return grpc::Status::OK;
    }
    
    grpc::Status ReceiveData(grpc::ServerContext* context,
                            const SubscribeRequest* request,
                            grpc::ServerWriter<EventBatchProto>* writer) override {
        // This would be implemented for merger -> sink streaming
        // For now, return OK
        return grpc::Status::OK;
    }
    
private:
    GrpcTransport* transport_;
};

GrpcTransport::GrpcTransport(ComponentType component)
    : componentType(component), isInitialized(false), isConnected(false), isShutdown(false),
      maxMessageSize(2147483647), keepaliveTimeMs(10000),
      messagesSent(0), messagesReceived(0), bytesSent(0), bytesReceived(0) {
}

GrpcTransport::~GrpcTransport() {
    Shutdown();
}

bool GrpcTransport::Initialize(const Config& config) {
    if (isInitialized.load()) {
        return true;
    }
    
    auto logger = Logger::GetLogger(componentType);
    
    // Store configuration
    maxMessageSize = config.GetGrpcConfig().maxMessageSize;
    keepaliveTimeMs = config.GetGrpcConfig().keepaliveTimeMs;
    
    bool success = false;
    switch (componentType) {
        case ComponentType::DATA_SENDER:
            success = InitializeAsSender(config);
            break;
        case ComponentType::DATA_RECEIVER:
            success = InitializeAsReceiver(config);
            break;
        case ComponentType::DATA_HUB:
            success = InitializeAsHub(config);
            break;
        default:
            logger->Error("Unknown component type for gRPC transport");
            return false;
    }
    
    if (success) {
        isInitialized.store(true);
        logger->Info("gRPC transport initialized successfully");
    } else {
        logger->Error("Failed to initialize gRPC transport");
    }
    
    return success;
}

bool GrpcTransport::InitializeAsSender(const Config& config) {
    // Sender connects to merger
    serverAddress = config.GetNetworkConfig().mergerAddress;
    
    auto channelArgs = CreateChannelArguments();
    channel = grpc::CreateCustomChannel(serverAddress, grpc::InsecureChannelCredentials(), channelArgs);
    
    if (!channel) {
        return false;
    }
    
    stub = DataStream::NewStub(channel);
    
    // Test connection
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    if (channel->WaitForConnected(deadline)) {
        isConnected.store(true);
        return true;
    }
    
    return false;
}

bool GrpcTransport::InitializeAsReceiver(const Config& config) {
    // Receiver connects to merger
    serverAddress = config.GetNetworkConfig().mergerAddress;
    
    auto channelArgs = CreateChannelArguments();
    channel = grpc::CreateCustomChannel(serverAddress, grpc::InsecureChannelCredentials(), channelArgs);
    
    if (!channel) {
        return false;
    }
    
    stub = DataStream::NewStub(channel);
    
    // Test connection
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    if (channel->WaitForConnected(deadline)) {
        isConnected.store(true);
        return true;
    }
    
    return false;
}

bool GrpcTransport::InitializeAsHub(const Config& config) {
    // Hub acts as server
    auto address = config.GetNetworkConfig().mergerAddress;
    
    serviceImpl = std::make_unique<DataStreamServiceImpl>(this);
    
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(serviceImpl.get());
    
    // Set message size limits
    builder.SetMaxReceiveMessageSize(maxMessageSize);
    builder.SetMaxSendMessageSize(maxMessageSize);
    
    server = builder.BuildAndStart();
    
    if (server) {
        isConnected.store(true);
        auto logger = Logger::GetLogger(componentType);
        logger->Info("gRPC server listening on %s", address.c_str());
        return true;
    }
    
    return false;
}

bool GrpcTransport::Send(const EventDataBatch& batch) {
    if (!isConnected.load() || isShutdown.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(sendMutex);
    
    try {
        EventBatchProto proto;
        if (!batch.SerializeToProtobuf(proto)) {
            return false;
        }
        
        // For senders, use client streaming
        if (componentType == ComponentType::DATA_SENDER) {
            if (!writer) {
                clientContext = std::make_unique<grpc::ClientContext>();
                Acknowledgment ack;
                writer = stub->SendData(clientContext.get(), &ack);
            }
            
            if (writer && writer->Write(proto)) {
                messagesSent.fetch_add(1);
                bytesSent.fetch_add(batch.GetDataSize());
                return true;
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("gRPC send error: %s", e.what());
        return false;
    }
}

bool GrpcTransport::Receive(EventDataBatch& batch) {
    if (!isConnected.load() || isShutdown.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(receiveMutex);
    
    try {
        // For receivers, use server streaming
        if (componentType == ComponentType::DATA_RECEIVER) {
            if (!reader) {
                clientContext = std::make_unique<grpc::ClientContext>();
                SubscribeRequest request;
                request.set_sink_id("sink_" + std::to_string(static_cast<int>(componentType)));
                reader = stub->ReceiveData(clientContext.get(), request);
            }
            
            if (reader) {
                EventBatchProto proto;
                if (reader->Read(&proto)) {
                    if (batch.DeserializeFromProtobuf(proto)) {
                        messagesReceived.fetch_add(1);
                        bytesReceived.fetch_add(batch.GetDataSize());
                        return true;
                    }
                }
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("gRPC receive error: %s", e.what());
        return false;
    }
}

void GrpcTransport::Shutdown() {
    if (isShutdown.load()) {
        return;
    }
    
    isShutdown.store(true);
    isConnected.store(false);
    
    // Close streams
    if (writer) {
        writer->WritesDone();
        writer.reset();
    }
    
    if (reader) {
        reader.reset();
    }
    
    if (clientContext) {
        clientContext.reset();
    }
    
    // Shutdown server
    if (server) {
        server->Shutdown();
        server.reset();
    }
    
    // Reset connection
    stub.reset();
    channel.reset();
    serviceImpl.reset();
}

std::string GrpcTransport::GetStats() const {
    std::ostringstream oss;
    oss << "gRPC Transport Stats:\n";
    oss << "  Messages Sent: " << messagesSent.load() << "\n";
    oss << "  Messages Received: " << messagesReceived.load() << "\n";
    oss << "  Bytes Sent: " << bytesSent.load() << "\n";
    oss << "  Bytes Received: " << bytesReceived.load() << "\n";
    oss << "  Connected: " << (isConnected.load() ? "Yes" : "No") << "\n";
    return oss.str();
}

bool GrpcTransport::IsConnected() const {
    return isConnected.load();
}

grpc::ChannelArguments GrpcTransport::CreateChannelArguments() const {
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(maxMessageSize);
    args.SetMaxSendMessageSize(maxMessageSize);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, keepaliveTimeMs);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
    args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    return args;
}

std::string GrpcTransport::BuildServerAddress(const std::string& host, uint16_t port) const {
    return host + ":" + std::to_string(port);
}

void GrpcTransport::OnBatchReceived(const EventDataBatch& batch) {
    // This would be called by the service implementation
    // For now, just update statistics
    messagesReceived.fetch_add(1);
    bytesReceived.fetch_add(batch.GetDataSize());
}


}} // namespace delila::test