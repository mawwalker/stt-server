#include "websocket_server.h"
#include "server_config.h"
#include "logger.h"
#include <iostream>
#include <string>
#include <signal.h>

using namespace std;

// Global server instance for signal handling
WebSocketASRServer* g_server = nullptr;

void signal_handler(int sig) {
    LOG_INFO("SERVER", "Received signal " << sig << ". Shutting down server...");
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // 创建配置管理器
    ServerConfig config;
    
    // 首先从环境变量加载配置
    config.load_from_environment();
    
    // 然后从命令行参数加载配置（会覆盖环境变量）
    config.load_from_args(argc, argv);
    
    // 验证配置
    if (!config.validate()) {
        LOG_ERROR("MAIN", "Invalid configuration");
        return 1;
    }
    
    const auto& server_settings = config.get_server_settings();
    
    // Set log level
    const std::string& log_level = server_settings.log_level;
    if (log_level == "DEBUG") Logger::set_level(LogLevel::DEBUG);
    else if (log_level == "INFO") Logger::set_level(LogLevel::INFO);
    else if (log_level == "WARN") Logger::set_level(LogLevel::WARN);
    else if (log_level == "ERROR") Logger::set_level(LogLevel::ERROR);
    else {
        cerr << "Invalid log level: " << log_level << endl;
        return 1;
    }
    
    LOG_INFO("SERVER", "Starting WebSocket ASR Server...");
    
    // 打印当前配置
    config.print_config();
    
    try {
        WebSocketASRServer server(config);
        g_server = &server;
        
        // Setup signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        if (!server.initialize()) {
            LOG_ERROR("SERVER", "Failed to initialize ASR server");
            return 1;
        }
        
        server.run();
        
    } catch (const exception& e) {
        LOG_ERROR("SERVER", "Server error: " << e.what());
        return 1;
    }
    
    return 0;
}
