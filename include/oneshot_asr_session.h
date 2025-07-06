#pragma once

#include "asr_engine.h"
#include "asr_result.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::connection_hdl connection_hdl;

class OneShotASRSession {
private:
    ASREngine* engine;
    connection_hdl hdl;
    server* ws_server;
    std::string client_id;
    std::atomic<bool> running;
    std::atomic<bool> recording;
    
    // 音频缓冲区
    std::vector<float> audio_buffer;
    std::mutex audio_mutex;
    
    // 会话状态
    enum class SessionState {
        WAITING_START,
        RECORDING,
        PROCESSING,
        FINISHED
    };
    std::atomic<SessionState> state;
    
    // Performance metrics
    std::chrono::steady_clock::time_point session_start_time;
    std::chrono::steady_clock::time_point recording_start_time;
    
public:
    OneShotASRSession(ASREngine* eng, connection_hdl h, server* srv, const std::string& id);
    ~OneShotASRSession();
    
    void start();
    void stop();
    void handle_message(const std::string& message);
    void add_audio_data(const std::vector<uint8_t>& pcm_bytes);
    
    std::string get_client_id() const;
    bool is_running() const;
    bool is_recording() const;

private:
    void start_recording();
    void stop_recording_and_process();
    void process_complete_audio();
    void send_result(const ASRResult& result);
    void send_error(const std::string& error_message);
    void send_status(const std::string& status);
};
