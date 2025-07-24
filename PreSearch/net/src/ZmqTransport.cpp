#include "ZmqTransport.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include <cstring>

namespace delila {
namespace test {

ZmqTransport::ZmqTransport(ComponentType component)
    : componentType(component), isInitialized(false), isConnected(false), isShutdown(false),
      highWaterMark(0), lingerMs(1000), rcvBufferSize(4194304),
      messagesSent(0), messagesReceived(0), bytesSent(0), bytesReceived(0) {
}

ZmqTransport::~ZmqTransport() {
    Shutdown();
}

bool ZmqTransport::Initialize(const Config& config) {
    if (isInitialized.load()) {
        return true;
    }
    
    auto logger = Logger::GetLogger(componentType);
    
    // Store configuration
    highWaterMark = config.GetZeroMqConfig().highWaterMark;
    lingerMs = config.GetZeroMqConfig().lingerMs;
    rcvBufferSize = config.GetZeroMqConfig().rcvBufferSize;
    
    // Create ZMQ context
    try {
        context = std::make_unique<zmq::context_t>(1);
        
        // Create socket based on component type
        int socketType = GetSocketType();
        socket = std::make_unique<zmq::socket_t>(*context, socketType);
        
        // Set socket options
        if (!SetSocketOptions()) {
            logger->Error("Failed to set ZMQ socket options");
            return false;
        }
        
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
                logger->Error("Unknown component type for ZMQ transport");
                return false;
        }
        
        if (success) {
            isInitialized.store(true);
            isConnected.store(true);
            logger->Info("ZMQ transport initialized successfully");
        } else {
            logger->Error("Failed to initialize ZMQ transport");
        }
        
        return success;
        
    } catch (const zmq::error_t& e) {
        logger->Error("ZMQ initialization error: %s", e.what());
        return false;
    }
}

int ZmqTransport::GetSocketType() const {
    switch (componentType) {
        case ComponentType::DATA_SENDER:
            return ZMQ_PUSH;  // PUSH to merger
        case ComponentType::DATA_RECEIVER:
            return ZMQ_SUB;   // SUB from merger
        case ComponentType::DATA_HUB:
            // Hub needs both PULL (from senders) and PUB (to receivers)
            // We'll handle this specially
            return ZMQ_PULL;
        default:
            return ZMQ_PAIR;
    }
}

bool ZmqTransport::SetSocketOptions() {
    try {
        // Set high water mark
        socket->set(zmq::sockopt::sndhwm, static_cast<int>(highWaterMark));
        socket->set(zmq::sockopt::rcvhwm, static_cast<int>(highWaterMark));
        
        // Set linger time
        socket->set(zmq::sockopt::linger, static_cast<int>(lingerMs));
        
        // Set receive buffer size
        socket->set(zmq::sockopt::rcvbuf, static_cast<int>(rcvBufferSize));
        
        // For SUB sockets, subscribe to DATA topic
        if (componentType == ComponentType::DATA_RECEIVER) {
            socket->set(zmq::sockopt::subscribe, DATA_TOPIC);
        }
        
        return true;
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("Failed to set ZMQ socket options: %s", e.what());
        return false;
    }
}

bool ZmqTransport::InitializeAsSender(const Config& config) {
    // Senders connect to merger
    address = BuildAddress("localhost", 3389); // Use merger port
    
    try {
        socket->connect(address);
        auto logger = Logger::GetLogger(componentType);
        logger->Info("ZMQ sender connected to %s", address.c_str());
        return true;
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("ZMQ sender connection failed: %s", e.what());
        return false;
    }
}

bool ZmqTransport::InitializeAsReceiver(const Config& config) {
    // Receivers connect to hub's PUB socket
    address = BuildAddress("localhost", config.GetNetworkConfig().hubPubPort);
    
    try {
        socket->connect(address);
        auto logger = Logger::GetLogger(componentType);
        logger->Info("ZMQ receiver connected to %s", address.c_str());
        return true;
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("ZMQ receiver connection failed: %s", e.what());
        return false;
    }
}

bool ZmqTransport::InitializeAsHub(const Config& config) {
    // Hub binds to receive from senders and publish to receivers
    auto pullAddress = BuildAddress("*", 3389);
    auto pubAddress = BuildAddress("*", config.GetNetworkConfig().hubPubPort);
    
    try {
        // Bind PULL socket for receiving from senders
        socket->bind(pullAddress);
        auto logger = Logger::GetLogger(componentType);
        logger->Info("ZMQ hub PULL socket bound to %s", pullAddress.c_str());
        
        // Create and bind PUB socket for publishing to receivers
        hubPubSocket = std::make_unique<zmq::socket_t>(*context, ZMQ_PUB);
        
        // Set socket options for PUB socket
        hubPubSocket->set(zmq::sockopt::sndhwm, static_cast<int>(highWaterMark));
        hubPubSocket->set(zmq::sockopt::linger, static_cast<int>(lingerMs));
        
        hubPubSocket->bind(pubAddress);
        logger->Info("ZMQ hub PUB socket bound to %s", pubAddress.c_str());
        
        return true;
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("ZMQ hub binding failed: %s", e.what());
        return false;
    }
}

bool ZmqTransport::Send(const EventDataBatch& batch) {
    if (!isConnected.load() || isShutdown.load()) {
        return false;
    }
    
    try {
        std::vector<uint8_t> data;
        if (!batch.SerializeToBinary(data)) {
            return false;
        }
        
        bool success = false;
        if (componentType == ComponentType::DATA_SENDER) {
            success = SendMessage(data);
        } else if (componentType == ComponentType::DATA_HUB) {
            // Hub forwards with topic for PUB socket
            ZmqEnvelope envelope;
            std::strncpy(envelope.topic, DATA_TOPIC, sizeof(envelope.topic) - 1);
            envelope.topic[sizeof(envelope.topic) - 1] = '\0';
            envelope.size = data.size();
            
            // Send envelope + data using hubPubSocket
            zmq::message_t topicMsg(sizeof(envelope));
            std::memcpy(topicMsg.data(), &envelope, sizeof(envelope));
            
            zmq::message_t dataMsg(data.size());
            std::memcpy(dataMsg.data(), data.data(), data.size());
            
            if (hubPubSocket) {
                auto result1 = hubPubSocket->send(topicMsg, zmq::send_flags::sndmore);
                auto result2 = hubPubSocket->send(dataMsg, zmq::send_flags::none);
                success = result1.has_value() && result2.has_value();
            } else {
                success = false;
            }
        }
        
        if (success) {
            messagesSent.fetch_add(1);
            bytesSent.fetch_add(data.size());
        }
        
        return success;
        
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("ZMQ send error: %s", e.what());
        return false;
    }
}

bool ZmqTransport::Receive(EventDataBatch& batch) {
    if (!isConnected.load() || isShutdown.load()) {
        return false;
    }
    
    try {
        std::vector<uint8_t> data;
        bool success = false;
        
        if (componentType == ComponentType::DATA_RECEIVER) {
            // Receive with topic
            zmq::message_t topicMsg;
            zmq::message_t dataMsg;
            
            if (socket->recv(topicMsg) && socket->recv(dataMsg)) {
                data.resize(dataMsg.size());
                std::memcpy(data.data(), dataMsg.data(), dataMsg.size());
                success = true;
            }
        } else {
            success = ReceiveMessage(data);
        }
        
        if (success && !data.empty()) {
            if (batch.DeserializeFromBinary(data)) {
                messagesReceived.fetch_add(1);
                bytesReceived.fetch_add(data.size());
                return true;
            }
        }
        
        return false;
        
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("ZMQ receive error: %s", e.what());
        return false;
    }
}

void ZmqTransport::Shutdown() {
    if (isShutdown.load()) {
        return;
    }
    
    isShutdown.store(true);
    isConnected.store(false);
    
    try {
        if (socket) {
            socket->close();
            socket.reset();
        }
        
        if (hubPubSocket) {
            hubPubSocket->close();
            hubPubSocket.reset();
        }
        
        if (context) {
            context.reset();
        }
        
        auto logger = Logger::GetLogger(componentType);
        logger->Info("ZMQ transport shut down");
        
    } catch (const zmq::error_t& e) {
        auto logger = Logger::GetLogger(componentType);
        logger->Error("ZMQ shutdown error: %s", e.what());
    }
}

std::string ZmqTransport::GetStats() const {
    std::ostringstream oss;
    oss << "ZMQ Transport Stats:\n";
    oss << "  Messages Sent: " << messagesSent.load() << "\n";
    oss << "  Messages Received: " << messagesReceived.load() << "\n";
    oss << "  Bytes Sent: " << bytesSent.load() << "\n";
    oss << "  Bytes Received: " << bytesReceived.load() << "\n";
    oss << "  Connected: " << (isConnected.load() ? "Yes" : "No") << "\n";
    return oss.str();
}

bool ZmqTransport::IsConnected() const {
    return isConnected.load();
}

std::string ZmqTransport::BuildAddress(const std::string& host, uint16_t port) const {
    return "tcp://" + host + ":" + std::to_string(port);
}

bool ZmqTransport::SendMessage(const std::vector<uint8_t>& data) {
    zmq::message_t message(data.size());
    std::memcpy(message.data(), data.data(), data.size());
    auto result = socket->send(message, zmq::send_flags::dontwait);
    return result.has_value();
}

bool ZmqTransport::ReceiveMessage(std::vector<uint8_t>& data) {
    zmq::message_t message;
    auto result = socket->recv(message, zmq::recv_flags::dontwait);
    if (result.has_value()) {
        data.resize(message.size());
        std::memcpy(data.data(), message.data(), message.size());
        return true;
    }
    return false;
}

}} // namespace delila::test