#pragma once

#include "Common.hpp"
#include <memory>

namespace delila {
namespace test {

// Forward declarations
class EventDataBatch;
class Config;

// Base transport interface
class ITransport {
public:
    virtual ~ITransport() = default;
    
    // Initialize transport with configuration
    virtual bool Initialize(const Config& config) = 0;
    
    // Send data batch
    virtual bool Send(const EventDataBatch& batch) = 0;
    
    // Receive data batch (blocking)
    virtual bool Receive(EventDataBatch& batch) = 0;
    
    // Shutdown transport
    virtual void Shutdown() = 0;
    
    // Get transport statistics
    virtual std::string GetStats() const = 0;
    
    // Check if transport is connected
    virtual bool IsConnected() const = 0;
};

// Transport factory
class TransportFactory {
public:
    static std::unique_ptr<ITransport> Create(TransportType type, ComponentType component);
};

}} // namespace delila::test