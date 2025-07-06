#pragma once

#include "asr_engine.h"
#include "asr_session.h"
#include "oneshot_asr_session.h"
#include "connection_manager.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>

// 前向声明
class ServerConfig;

typedef websocketpp::server<websocketpp::config::asio> server;
typedef server::message_ptr message_ptr;
typedef websocketpp::connection_hdl connection_hdl;

class WebSocketASRServer {
private:
    server ws_server;
    ASREngine asr_engine;
    ConnectionManager connection_manager;
    std::unordered_map<std::string, std::unique_ptr<ASRSession>> sessions;
    std::unordered_map<std::string, std::unique_ptr<OneShotASRSession>> oneshot_sessions;
    std::mutex sessions_mutex;
    std::mutex oneshot_sessions_mutex;
    std::unique_ptr<ServerConfig> config_;
    std::atomic<size_t> total_connections{0};
    std::atomic<size_t> active_sessions{0};
    std::atomic<size_t> active_oneshot_sessions{0};
    
    // Performance monitoring
    std::thread monitor_thread;
    std::atomic<bool> monitoring{false};
    
public:
    WebSocketASRServer(const ServerConfig& config);
    ~WebSocketASRServer();
    
    bool initialize();
    void run();
    void stop();
    
private:
    void start_monitoring();
    void stop_monitoring();
    void monitor_performance();
    
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, message_ptr msg);
    
    // Helper methods to determine session type based on URI
    bool is_oneshot_endpoint(connection_hdl hdl);
    std::string get_endpoint_path(connection_hdl hdl);
};
