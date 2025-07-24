#pragma once

#include "Common.hpp"
#include <vector>
#include <string>
#include <memory>

namespace delila {
namespace test {

// Network configuration
struct NetworkConfig {
    std::string mergerAddress = "localhost:3389";
    uint16_t hubPubPort = 3388;
    uint16_t source1Port = 3390;
    uint16_t source2Port = 3391;
    uint16_t sink1Port = 3392;
    uint16_t sink2Port = 3393;
};

// gRPC specific configuration
struct GrpcConfig {
    uint32_t maxMessageSize = 2147483647; // 2GB
    uint32_t keepaliveTimeMs = 10000;
};

// ZeroMQ specific configuration
struct ZeroMqConfig {
    uint32_t highWaterMark = 0; // unlimited
    uint32_t lingerMs = 1000;
    uint32_t rcvBufferSize = 4194304; // 4MB
};

// Logging configuration
struct LoggingConfig {
    std::string level = "info";
    std::string directory = "./logs";
};

// Test configuration
struct TestConfig {
    TransportType protocol = TransportType::GRPC;
    NetworkType transport = NetworkType::TCP;
    uint32_t durationMinutes = 10;
    std::vector<uint32_t> batchSizes = {1024, 10240, 20480, 51200, 102400, 204800, 512000, 1048576, 2097152, 5242880, 10485760};
    std::string outputDir = "./results";
};

// Main configuration class
class Config {
public:
    // Constructor
    Config();
    
    // Load configuration from JSON file
    bool LoadFromFile(const std::string& filename);
    
    // Save configuration to JSON file
    bool SaveToFile(const std::string& filename) const;
    
    // Validate configuration
    bool IsValid() const;
    
    // Getters
    const TestConfig& GetTestConfig() const { return testConfig; }
    const NetworkConfig& GetNetworkConfig() const { return networkConfig; }
    const GrpcConfig& GetGrpcConfig() const { return grpcConfig; }
    const ZeroMqConfig& GetZeroMqConfig() const { return zmqConfig; }
    const LoggingConfig& GetLoggingConfig() const { return loggingConfig; }
    
    // Setters
    TestConfig& GetTestConfig() { return testConfig; }
    NetworkConfig& GetNetworkConfig() { return networkConfig; }
    GrpcConfig& GetGrpcConfig() { return grpcConfig; }
    ZeroMqConfig& GetZeroMqConfig() { return zmqConfig; }
    LoggingConfig& GetLoggingConfig() { return loggingConfig; }
    
    // Utility methods
    std::string GetProtocolString() const;
    std::string GetTransportString() const;
    std::string GetLogFilePath(ComponentType component) const;
    std::string GetResultsFilePath(const std::string& filename) const;
    
private:
    TestConfig testConfig;
    NetworkConfig networkConfig;
    GrpcConfig grpcConfig;
    ZeroMqConfig zmqConfig;
    LoggingConfig loggingConfig;
    
    // JSON parsing helpers
    bool ParseJsonValue(const class JsonValue& value);
    bool ParseTestConfig(const class JsonValue& value);
    bool ParseNetworkConfig(const class JsonValue& value);
    bool ParseGrpcConfig(const class JsonValue& value);
    bool ParseZeroMqConfig(const class JsonValue& value);
    bool ParseLoggingConfig(const class JsonValue& value);
};

}} // namespace delila::test