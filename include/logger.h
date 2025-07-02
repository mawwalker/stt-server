#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
private:
    static LogLevel current_level;
    static std::mutex log_mutex;
    
    static std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    static std::string get_thread_id() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }
    
    static std::string level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
public:
    static void set_level(LogLevel level) {
        current_level = level;
    }
    
    static void log(LogLevel level, const std::string& client_id, 
                   const std::string& file, int line, const std::string& message) {
        if (level < current_level) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[" << get_timestamp() << "] "
                  << "[" << level_to_string(level) << "] "
                  << "[" << get_thread_id() << "] "
                  << "[" << client_id << "] "
                  << "[" << file << ":" << line << "] "
                  << message << std::endl;
    }
};

// Convenience macros
#define LOG_DEBUG(client_id, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        Logger::log(LogLevel::DEBUG, client_id, __FILE__, __LINE__, ss.str()); \
    } while(0)

#define LOG_INFO(client_id, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        Logger::log(LogLevel::INFO, client_id, __FILE__, __LINE__, ss.str()); \
    } while(0)

#define LOG_WARN(client_id, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        Logger::log(LogLevel::WARN, client_id, __FILE__, __LINE__, ss.str()); \
    } while(0)

#define LOG_ERROR(client_id, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        Logger::log(LogLevel::ERROR, client_id, __FILE__, __LINE__, ss.str()); \
    } while(0)
