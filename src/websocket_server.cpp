#include "websocket_server.h"
#include "logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

WebSocketASRServer::WebSocketASRServer(const std::string& models_path, int server_port) 
    : model_dir(models_path), port(server_port) {
    
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
    
    LOG_INFO("SERVER", "WebSocket ASR Server initialized with models: " << models_path << ", port: " << server_port);
}

WebSocketASRServer::~WebSocketASRServer() {
    stop_monitoring();
}

bool WebSocketASRServer::initialize() {
    LOG_INFO("SERVER", "Initializing ASR engine...");
    bool success = asr_engine.initialize(model_dir);
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
    
    try {
        ws_server.listen(port);
        ws_server.start_accept();
        
        LOG_INFO("SERVER", "WebSocket ASR server listening on port " << port);
        LOG_INFO("SERVER", "WebSocket endpoint: ws://localhost:" << port << "/asr");
        
        ws_server.run();
    } catch (const std::exception& e) {
        LOG_ERROR("SERVER", "Error running server: " << e.what());
    }
}

void WebSocketASRServer::stop() {
    LOG_INFO("SERVER", "Stopping WebSocket ASR server...");
    stop_monitoring();
    
    // Stop all sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        for (auto& pair : sessions) {
            if (pair.second) {
                pair.second->stop();
            }
        }
        sessions.clear();
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
        
        // 获取ASR池状态
        auto pool_stats = asr_engine.get_model_manager()->get_asr_pool_stats();
        
        LOG_INFO("SERVER", "Performance stats - Total connections: " << total_connections.load() 
                 << ", Active connections: " << connections 
                 << ", Active sessions: " << sessions_count
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
    
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto session = std::make_unique<ASRSession>(&asr_engine, hdl, &ws_server, client_id);
    session->start();
    sessions[client_id] = std::move(session);
    active_sessions++;
    
    LOG_INFO(client_id, "New WebSocket connection opened. Total connections: " 
             << connection_manager.get_connection_count());
}

void WebSocketASRServer::on_close(connection_hdl hdl) {
    std::string client_id = connection_manager.get_client_id(hdl);
    connection_manager.remove_connection(hdl);
    
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = sessions.find(client_id);
    if (it != sessions.end()) {
        if (it->second) {
            it->second->stop();
            active_sessions--;
        }
        sessions.erase(it);
    }
    
    LOG_INFO(client_id, "WebSocket connection closed. Remaining connections: " 
             << connection_manager.get_connection_count());
}

void WebSocketASRServer::on_message(connection_hdl hdl, message_ptr msg) {
    std::string client_id = connection_manager.get_client_id(hdl);
    
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = sessions.find(client_id);
    if (it != sessions.end() && it->second) {
        if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
            const std::string& payload = msg->get_payload();
            std::vector<uint8_t> pcm_data(payload.begin(), payload.end());
            it->second->add_audio_data(pcm_data);
            LOG_DEBUG(client_id, "Received " << pcm_data.size() << " bytes of audio data");
        } else {
            LOG_WARN(client_id, "Received non-binary message, ignoring");
        }
    } else {
        LOG_WARN(client_id, "Received message for unknown session");
    }
}
