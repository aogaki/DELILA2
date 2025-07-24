#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <ctime>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

namespace delila {
namespace test {

std::string Logger::logDirectory = "./logs";
LogLevel Logger::globalLogLevel = LogLevel::INFO;

std::shared_ptr<Logger> Logger::GetLogger(ComponentType component) {
    static std::map<ComponentType, std::shared_ptr<Logger>> loggers;
    static std::mutex loggerMutex;
    
    std::lock_guard<std::mutex> lock(loggerMutex);
    
    auto it = loggers.find(component);
    if (it != loggers.end()) {
        return it->second;
    }
    
    std::string logFile = logDirectory + "/" + ComponentTypeToString(component) + ".log";
    auto logger = std::shared_ptr<Logger>(new Logger(component, logFile));
    loggers[component] = logger;
    return logger;
}

bool Logger::Initialize(const std::string& logDir, LogLevel level) {
    logDirectory = logDir;
    globalLogLevel = level;
    
    // Create log directory if it doesn't exist
    if (mkdir(logDir.c_str(), 0755) != 0 && errno != EEXIST) {
        std::cerr << "Failed to create log directory: " << logDir << std::endl;
        return false;
    }
    return true;
}

Logger::Logger(ComponentType component, const std::string& logFile) 
    : component(component), currentLevel(globalLogLevel) {
    
    this->logFile.open(logFile, std::ios::out | std::ios::app);
    if (!this->logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFile << std::endl;
    }
    
    // Write session start marker
    WriteLog(LogLevel::INFO, "=== Logging session started ===");
}

Logger::~Logger() {
    WriteLog(LogLevel::INFO, "=== Logging session ended ===");
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::Debug(const std::string& message) {
    WriteLog(LogLevel::DEBUG, message);
}

void Logger::Info(const std::string& message) {
    WriteLog(LogLevel::INFO, message);
}

void Logger::Warning(const std::string& message) {
    WriteLog(LogLevel::WARNING, message);
}

void Logger::Error(const std::string& message) {
    WriteLog(LogLevel::ERROR, message);
}

void Logger::SetLogLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.flush();
    }
}

void Logger::WriteLog(LogLevel level, const std::string& message) {
    if (level < currentLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::string timestamp = GetTimestamp();
    std::string levelStr = LogLevelToString(level);
    std::string componentStr = ComponentTypeToString(component);
    
    std::string logEntry = "[" + timestamp + "] [" + levelStr + "] [" + componentStr + "] " + message;
    
    // Write to file
    if (logFile.is_open()) {
        logFile << logEntry << std::endl;
    }
    
    // Also write to console for ERROR level
    if (level == LogLevel::ERROR) {
        std::cerr << logEntry << std::endl;
    }
}

std::string Logger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::LogLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

}} // namespace delila::test