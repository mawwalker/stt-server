#include "logger.h"

// Static member definitions
LogLevel Logger::current_level = LogLevel::INFO;
std::mutex Logger::log_mutex;
