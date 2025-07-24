#pragma once

#include "Common.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace delila {
namespace test {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    // Get logger instance for component
    static std::shared_ptr<Logger> GetLogger(ComponentType component);
    
    // Initialize logging system
    static bool Initialize(const std::string& logDir, LogLevel level = LogLevel::INFO);
    
    // Log methods
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);
    
    // Log with format
    template<typename... Args>
    void Debug(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void Info(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void Warning(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void Error(const std::string& format, Args&&... args);
    
    // Set log level
    void SetLogLevel(LogLevel level);
    
    // Flush logs
    void Flush();
    
    // Destructor (public for shared_ptr)
    ~Logger();
    
private:
    Logger(ComponentType component, const std::string& logFile);
    
    void WriteLog(LogLevel level, const std::string& message);
    std::string GetTimestamp() const;
    std::string LogLevelToString(LogLevel level) const;
    
    ComponentType component;
    std::ofstream logFile;
    LogLevel currentLevel;
    std::mutex logMutex;
    
    static std::string logDirectory;
    static LogLevel globalLogLevel;
};

// Template implementations
template<typename... Args>
void Logger::Debug(const std::string& format, Args&&... args) {
    // Simple sprintf-like implementation
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format.c_str(), args...);
    Debug(std::string(buffer));
}

template<typename... Args>
void Logger::Info(const std::string& format, Args&&... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format.c_str(), args...);
    Info(std::string(buffer));
}

template<typename... Args>
void Logger::Warning(const std::string& format, Args&&... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format.c_str(), args...);
    Warning(std::string(buffer));
}

template<typename... Args>
void Logger::Error(const std::string& format, Args&&... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format.c_str(), args...);
    Error(std::string(buffer));
}

}} // namespace delila::test