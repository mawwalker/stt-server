#include "websocket_server.h"
#include "logger.h"
#include <iostream>
#include <string>
#include <signal.h>

using namespace std;

void print_usage(const char* program_name) {
    cout << "Usage: " << program_name << " [options]" << endl;
    cout << "Options:" << endl;
    cout << "  --port PORT          Server port (default: 8000)" << endl;
    cout << "  --models-root PATH   Path to models directory (default: ./assets)" << endl;
    cout << "  --threads NUM        Number of threads (default: 2)" << endl;
    cout << "  --log-level LEVEL    Log level: DEBUG, INFO, WARN, ERROR (default: INFO)" << endl;
    cout << "  --help               Show this help message" << endl;
}

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
    string models_root = "./assets";
    int port = 8000;
    int num_threads = 2;
    string log_level = "INFO";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        } else if (arg == "--models-root" && i + 1 < argc) {
            models_root = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            num_threads = stoi(argv[++i]);
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else {
            cerr << "Unknown argument: " << arg << endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Set log level
    if (log_level == "DEBUG") Logger::set_level(LogLevel::DEBUG);
    else if (log_level == "INFO") Logger::set_level(LogLevel::INFO);
    else if (log_level == "WARN") Logger::set_level(LogLevel::WARN);
    else if (log_level == "ERROR") Logger::set_level(LogLevel::ERROR);
    else {
        cerr << "Invalid log level: " << log_level << endl;
        return 1;
    }
    
    LOG_INFO("SERVER", "Starting WebSocket ASR Server...");
    LOG_INFO("SERVER", "Models directory: " << models_root);
    LOG_INFO("SERVER", "Port: " << port);
    LOG_INFO("SERVER", "Threads: " << num_threads);
    LOG_INFO("SERVER", "Log level: " << log_level);
    
    try {
        WebSocketASRServer server(models_root, port);
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
