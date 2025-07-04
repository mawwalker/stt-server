#pragma once

#include "asr_engine.h"
#include "asr_result.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::connection_hdl connection_hdl;

class ASRSession {
private:
    ASREngine* engine;
    connection_hdl hdl;
    server* ws_server;
    std::string client_id;
    std::atomic<bool> running;
    std::thread processing_thread;
    
    std::queue<std::vector<float>> audio_queue;
    std::mutex audio_mutex;
    std::condition_variable audio_cv;
    
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad;
    std::atomic<int> segment_id;
    std::vector<float> buffer;
    std::atomic<int> offset;
    std::atomic<bool> speech_started;
    std::chrono::steady_clock::time_point started_time;
    static const int window_size = 512;
    
    // ASR实例管理
    std::atomic<int> acquired_asr_instance{-1};
    
    // Performance metrics
    std::atomic<size_t> processed_samples{0};
    std::atomic<size_t> processed_segments{0};
    std::chrono::steady_clock::time_point session_start_time;
    
public:
    ASRSession(ASREngine* eng, connection_hdl h, server* srv, const std::string& id);
    ~ASRSession();
    
    void start();
    void stop();
    void add_audio_data(const std::vector<uint8_t>& pcm_bytes);
    
    std::string get_client_id() const;
    bool is_running() const;

private:
    void process_audio();
    void perform_recognition(bool is_final);
    void process_speech_segment(const sherpa_onnx::cxx::SpeechSegment& segment);
    void send_result(const ASRResult& result);
};
