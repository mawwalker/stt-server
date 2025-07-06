#include "websocket_server.h"
#include "oneshot_asr_session.h"
#include "server_config.h"
#include "logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

WebSocketASRServer::WebSocketASRServer(const ServerConfig& config) 
    : config_(std::make_unique<ServerConfig>(config)) {
    
    // Initialize WebSocket server
    // ws_server.set_access_channels(websocketpp::log::alevel::all);
    ws_server.set_access_channels(websocketpp::log::alevel::connect | 
                         websocketpp::log::alevel::disconnect | 
                         websocketpp::log::alevel::app);
    ws_server.clear_access_channels(websocketpp::log::alevel::frame_payload | 
                           websocketpp::log::alevel::frame_header);
    ws_server.set_error_channels(websocketpp::log::elevel::warn | 
                websocketpp::log::elevel::rerror | 
                websocketpp::log::elevel::fatal);
    ws_server.init_asio();
    
    // Set handlers
    ws_server.set_message_handler([this](connection_hdl hdl, message_ptr msg) {
        on_message(hdl, msg);
    });
    
    ws_server.set_open_handler([this](connection_hdl hdl) {
        on_open(hdl);
    });
    
    ws_server.set_close_handler([this](connection_hdl hdl) {
        on_close(hdl);
    });
    
    ws_server.set_reuse_addr(true);
    
    const auto& server_settings = config_->get_server_settings();
    LOG_INFO("SERVER", "WebSocket ASR Server initialized with models: " 
             << server_settings.models_root << ", port: " << server_settings.port);
}

WebSocketASRServer::~WebSocketASRServer() {
    stop_monitoring();
}

bool WebSocketASRServer::initialize() {
    LOG_INFO("SERVER", "Initializing ASR engine...");
    const auto& server_settings = config_->get_server_settings();
    bool success = asr_engine.initialize(server_settings.models_root, *config_);
    if (success) {
        LOG_INFO("SERVER", "ASR engine initialized successfully");
        start_monitoring();
    } else {
        LOG_ERROR("SERVER", "Failed to initialize ASR engine");
    }
    return success;
}

void WebSocketASRServer::run() {
    if (!asr_engine.is_initialized()) {
        LOG_ERROR("SERVER", "ASR engine not initialized, cannot start server");
        return;
    }
    
    const auto& server_settings = config_->get_server_settings();
    
    try {
        ws_server.listen(server_settings.port);
        ws_server.start_accept();
        
        LOG_INFO("SERVER", "WebSocket ASR server listening on port " << server_settings.port);
        LOG_INFO("SERVER", "Streaming ASR endpoint: ws://localhost:" << server_settings.port << "/sttRealtime");
        LOG_INFO("SERVER", "OneShot ASR endpoint: ws://localhost:" << server_settings.port << "/oneshot");
        
        ws_server.run();
    } catch (const std::exception& e) {
        LOG_ERROR("SERVER", "Error running server: " << e.what());
    }
}

void WebSocketASRServer::stop() {
    LOG_INFO("SERVER", "Stopping WebSocket ASR server...");
    stop_monitoring();
    
    // Stop all streaming sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        for (auto& pair : sessions) {
            if (pair.second) {
                pair.second->stop();
            }
        }
        sessions.clear();
    }
    
    // Stop all oneshot sessions
    {
        std::lock_guard<std::mutex> lock(oneshot_sessions_mutex);
        for (auto& pair : oneshot_sessions) {
            if (pair.second) {
                pair.second->stop();
            }
        }
        oneshot_sessions.clear();
    }
    
    ws_server.stop();
    LOG_INFO("SERVER", "WebSocket ASR server stopped");
}

void WebSocketASRServer::start_monitoring() {
    monitoring = true;
    monitor_thread = std::thread(&WebSocketASRServer::monitor_performance, this);
    LOG_INFO("SERVER", "Performance monitoring started");
}

void WebSocketASRServer::stop_monitoring() {
    if (monitoring.exchange(false)) {
        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }
        LOG_INFO("SERVER", "Performance monitoring stopped");
    }
}

void WebSocketASRServer::monitor_performance() {
    while (monitoring) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        if (!monitoring) break;
        
        size_t connections = connection_manager.get_connection_count();
        size_t sessions_count = active_sessions.load();
        size_t oneshot_sessions_count = active_oneshot_sessions.load();
        
        // 获取ASR池状态
        auto pool_stats = asr_engine.get_model_manager()->get_asr_pool_stats();
        
        LOG_INFO("SERVER", "Performance stats - Total connections: " << total_connections.load() 
                 << ", Active connections: " << connections 
                 << ", Active streaming sessions: " << sessions_count
                 << ", Active oneshot sessions: " << oneshot_sessions_count
                 << ", ASR pool (total/available/in_use): " 
                 << pool_stats.total_instances << "/" 
                 << pool_stats.available_instances << "/" 
                 << pool_stats.in_use_instances);
        
        // 如果池使用率过高，发出警告
        if (pool_stats.available_instances == 0 && pool_stats.total_instances > 0) {
            LOG_WARN("SERVER", "ASR pool fully utilized - consider increasing pool size");
        }
        
        // Log individual session stats
        std::vector<std::string> client_ids = connection_manager.get_all_client_ids();
        for (const auto& client_id : client_ids) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            auto it = sessions.find(client_id);
            if (it != sessions.end() && it->second && it->second->is_running()) {
                LOG_DEBUG("SERVER", "Session " << client_id << " is active");
            }
        }
    }
}

void WebSocketASRServer::on_open(connection_hdl hdl) {
    std::string client_id = connection_manager.add_connection(hdl);
    total_connections++;
    
    // 检查连接的端点类型
    bool is_oneshot = is_oneshot_endpoint(hdl);
    std::string endpoint_path = get_endpoint_path(hdl);
    
    if (is_oneshot) {
        // 创建一句话识别会话
        std::lock_guard<std::mutex> lock(oneshot_sessions_mutex);
        auto session = std::make_unique<OneShotASRSession>(&asr_engine, hdl, &ws_server, client_id);
        session->start();
        oneshot_sessions[client_id] = std::move(session);
        active_oneshot_sessions++;
        
        LOG_INFO(client_id, "New OneShot WebSocket connection opened on " << endpoint_path 
                 << ". Total connections: " << connection_manager.get_connection_count());
    } else {
        // 创建流式识别会话
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto session = std::make_unique<ASRSession>(&asr_engine, hdl, &ws_server, client_id);
        session->start();
        sessions[client_id] = std::move(session);
        active_sessions++;
        
        LOG_INFO(client_id, "New Streaming WebSocket connection opened on " << endpoint_path 
                 << ". Total connections: " << connection_manager.get_connection_count());
    }
}

void WebSocketASRServer::on_close(connection_hdl hdl) {
    std::string client_id = connection_manager.get_client_id(hdl);
    connection_manager.remove_connection(hdl);
    
    // 首先尝试从流式会话中移除
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(client_id);
        if (it != sessions.end()) {
            if (it->second) {
                it->second->stop();
                active_sessions--;
            }
            sessions.erase(it);
            LOG_INFO(client_id, "Streaming WebSocket connection closed. Remaining connections: " 
                     << connection_manager.get_connection_count());
            return;
        }
    }
    
    // 然后尝试从一句话识别会话中移除
    {
        std::lock_guard<std::mutex> lock(oneshot_sessions_mutex);
        auto it = oneshot_sessions.find(client_id);
        if (it != oneshot_sessions.end()) {
            if (it->second) {
                it->second->stop();
                active_oneshot_sessions--;
            }
            oneshot_sessions.erase(it);
            LOG_INFO(client_id, "OneShot WebSocket connection closed. Remaining connections: " 
                     << connection_manager.get_connection_count());
            return;
        }
    }
    
    LOG_WARN(client_id, "Connection closed but session not found");
}

void WebSocketASRServer::on_message(connection_hdl hdl, message_ptr msg) {
    std::string client_id = connection_manager.get_client_id(hdl);
    
    // 首先尝试处理流式会话消息
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(client_id);
        if (it != sessions.end() && it->second) {
            if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                const std::string& payload = msg->get_payload();
                std::vector<uint8_t> pcm_data(payload.begin(), payload.end());
                it->second->add_audio_data(pcm_data);
                LOG_DEBUG(client_id, "Received " << pcm_data.size() << " bytes of audio data for streaming");
            } else {
                LOG_WARN(client_id, "Received non-binary message for streaming session, ignoring");
            }
            return;
        }
    }
    
    // 然后尝试处理一句话识别会话消息
    {
        std::lock_guard<std::mutex> lock(oneshot_sessions_mutex);
        auto it = oneshot_sessions.find(client_id);
        if (it != oneshot_sessions.end() && it->second) {
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                // 处理控制消息（start/stop）
                const std::string& payload = msg->get_payload();
                it->second->handle_message(payload);
                LOG_DEBUG(client_id, "Received control message for oneshot: " << payload);
            } else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                // 处理音频数据
                const std::string& payload = msg->get_payload();
                std::vector<uint8_t> pcm_data(payload.begin(), payload.end());
                it->second->add_audio_data(pcm_data);
                LOG_DEBUG(client_id, "Received " << pcm_data.size() << " bytes of audio data for oneshot");
            }
            return;
        }
    }
    
    LOG_WARN(client_id, "Received message for unknown session");
}

bool WebSocketASRServer::is_oneshot_endpoint(connection_hdl hdl) {
    try {
        auto con = ws_server.get_con_from_hdl(hdl);
        std::string uri = con->get_uri()->get_resource();
        return uri.find("/oneshot") != std::string::npos;
    } catch (const std::exception& e) {
        LOG_ERROR("SERVER", "Error checking endpoint: " << e.what());
        return false;
    }
}

std::string WebSocketASRServer::get_endpoint_path(connection_hdl hdl) {
    try {
        auto con = ws_server.get_con_from_hdl(hdl);
        return con->get_uri()->get_resource();
    } catch (const std::exception& e) {
        LOG_ERROR("SERVER", "Error getting endpoint path: " << e.what());
        return "/unknown";
    }
}
