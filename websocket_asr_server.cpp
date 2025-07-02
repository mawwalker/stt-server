#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <json/json.h>
#include <sherpa-onnx/c-api/cxx-api.h>
#include "logger.h"
#include "connection_manager.h"

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <functional>
#include <signal.h>
#include <cstring>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace sherpa_onnx::cxx;

typedef websocketpp::server<websocketpp::config::asio> server;
typedef server::message_ptr message_ptr;
typedef websocketpp::connection_hdl connection_hdl;

struct ASRResult {
    string text;
    bool finished;
    int idx;
    string lang;
    string emotion;
    string event;
    vector<float> timestamps;
    vector<string> tokens;
    
    Json::Value to_json() const {
        Json::Value result;
        result["text"] = text;
        result["finished"] = finished;
        result["idx"] = idx;
        result["lang"] = lang;
        result["emotion"] = emotion;
        result["event"] = event;
        
        Json::Value timestamps_array(Json::arrayValue);
        for (float timestamp : timestamps) {
            timestamps_array.append(timestamp);
        }
        result["timestamps"] = timestamps_array;
        
        Json::Value tokens_array(Json::arrayValue);
        for (const string& token : tokens) {
            tokens_array.append(token);
        }
        result["tokens"] = tokens_array;
        
        return result;
    }
};

class ASREngine {
private:
    std::unique_ptr<OfflineRecognizer> recognizer;
    std::unique_ptr<VoiceActivityDetector> vad_template;
    float sample_rate;
    std::atomic<bool> initialized;
    mutable std::mutex engine_mutex;
    string model_directory;
    
public:
    ASREngine() : sample_rate(16000), initialized(false) {}
    
    bool initialize(const string& model_dir, int num_threads = 2) {
        std::lock_guard<std::mutex> lock(engine_mutex);
        model_directory = model_dir;  // Store model directory
        try {
            // Initialize VAD template
            VadModelConfig vad_config;
            vad_config.silero_vad.model = model_dir + "/silero_vad/silero_vad.onnx";
            vad_config.silero_vad.threshold = 0.5;
            vad_config.silero_vad.min_silence_duration = 0.25;
            vad_config.silero_vad.min_speech_duration = 0.25;
            vad_config.silero_vad.max_speech_duration = 8;
            vad_config.sample_rate = sample_rate;
            vad_config.debug = false;
            
            auto vad_obj = VoiceActivityDetector::Create(vad_config, 100);
            if (!vad_obj.Get()) {
                LOG_ERROR("ENGINE", "Failed to create VAD template");
                return false;
            }
            vad_template = std::make_unique<VoiceActivityDetector>(std::move(vad_obj));
            
            // Initialize ASR
            OfflineRecognizerConfig config;
            config.model_config.sense_voice.model = 
                model_dir + "/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.onnx";
            config.model_config.sense_voice.use_itn = true;
            config.model_config.sense_voice.language = "auto";
            config.model_config.tokens = 
                model_dir + "/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt";
            config.model_config.num_threads = num_threads;
            config.model_config.debug = false;
            
            LOG_INFO("ENGINE", "Loading ASR model...");
            auto recognizer_obj = OfflineRecognizer::Create(config);
            if (!recognizer_obj.Get()) {
                LOG_ERROR("ENGINE", "Failed to create recognizer");
                return false;
            }
            recognizer = std::make_unique<OfflineRecognizer>(std::move(recognizer_obj));
            LOG_INFO("ENGINE", "ASR model loaded successfully");
            
            initialized = true;
            return true;
        } catch (const exception& e) {
            LOG_ERROR("ENGINE", "Error initializing ASR engine: " << e.what());
            return false;
        }
    }
    
    bool is_initialized() const { return initialized.load(); }
    float get_sample_rate() const { return sample_rate; }
    
    // Create a new VAD instance for each session
    std::unique_ptr<VoiceActivityDetector> create_vad() const {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!vad_template || model_directory.empty()) return nullptr;
        
        VadModelConfig vad_config;
        vad_config.silero_vad.model = model_directory + "/silero_vad/silero_vad.onnx";  // Set model path
        vad_config.silero_vad.threshold = 0.5;
        vad_config.silero_vad.min_silence_duration = 0.25;
        vad_config.silero_vad.min_speech_duration = 0.25;
        vad_config.silero_vad.max_speech_duration = 8;
        vad_config.sample_rate = sample_rate;
        vad_config.debug = false;
        
        // Create new VAD instance with proper model path
        auto vad_obj = VoiceActivityDetector::Create(vad_config, 100);
        if (!vad_obj.Get()) return nullptr;
        
        return std::make_unique<VoiceActivityDetector>(std::move(vad_obj));
    }
    
    OfflineRecognizer& get_recognizer() { 
        std::lock_guard<std::mutex> lock(engine_mutex);
        return *recognizer; 
    }
};

class ASRSession {
private:
    ASREngine* engine;
    connection_hdl hdl;
    server* ws_server;
    string client_id;
    std::atomic<bool> running;
    std::thread processing_thread;
    
    std::queue<vector<float>> audio_queue;
    std::mutex audio_mutex;
    std::condition_variable audio_cv;
    
    std::unique_ptr<VoiceActivityDetector> vad;
    std::atomic<int> segment_id;
    vector<float> buffer;
    std::atomic<int> offset;
    std::atomic<bool> speech_started;
    std::chrono::steady_clock::time_point started_time;
    static const int window_size = 512;
    
    // Performance metrics
    std::atomic<size_t> processed_samples{0};
    std::atomic<size_t> processed_segments{0};
    std::chrono::steady_clock::time_point session_start_time;
    
public:
    ASRSession(ASREngine* eng, connection_hdl h, server* srv, const string& id) 
        : engine(eng), hdl(h), ws_server(srv), client_id(id), running(true), 
          segment_id(0), offset(0), speech_started(false),
          session_start_time(std::chrono::steady_clock::now()) {
        
        // Create a dedicated VAD instance for this session
        vad = engine->create_vad();
        if (!vad) {
            LOG_ERROR(client_id, "Failed to create VAD for session");
            running = false;
        }
    }
    
    ~ASRSession() {
        stop();
        auto session_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - session_start_time).count();
        LOG_INFO(client_id, "Session ended. Duration: " << session_duration 
                 << "s, Processed samples: " << processed_samples.load() 
                 << ", Segments: " << processed_segments.load());
    }
    
    void start() {
        if (!vad || !running) {
            LOG_ERROR(client_id, "Cannot start session - VAD not available");
            return;
        }
        LOG_INFO(client_id, "Starting ASR session");
        processing_thread = std::thread(&ASRSession::process_audio, this);
    }
    
    void stop() {
        if (running.exchange(false)) {
            LOG_INFO(client_id, "Stopping ASR session");
            audio_cv.notify_all();
            if (processing_thread.joinable()) {
                processing_thread.join();
            }
        }
    }
    
    void add_audio_data(const vector<uint8_t>& pcm_bytes) {
        if (!running) return;
        
        // Convert PCM bytes to float samples
        vector<float> samples;
        samples.reserve(pcm_bytes.size() / 2);
        
        for (size_t i = 0; i < pcm_bytes.size(); i += 2) {
            if (i + 1 < pcm_bytes.size()) {
                int16_t sample = static_cast<int16_t>(pcm_bytes[i] | (pcm_bytes[i + 1] << 8));
                samples.push_back(static_cast<float>(sample) / 32768.0f);
            }
        }
        
        processed_samples += samples.size();
        
        {
            std::lock_guard<std::mutex> lock(audio_mutex);
            audio_queue.push(std::move(samples));
        }
        audio_cv.notify_one();
        
        LOG_DEBUG(client_id, "Added " << samples.size() << " audio samples to queue");
    }
    
    string get_client_id() const { return client_id; }
    bool is_running() const { return running.load(); }

private:
    void process_audio() {
        if (!vad) {
            LOG_ERROR(client_id, "VAD not available for processing");
            return;
        }
        
        LOG_INFO(client_id, "ASR session processing started");
        
        while (running) {
            vector<float> samples;
            
            // Wait for audio data
            {
                std::unique_lock<std::mutex> lock(audio_mutex);
                audio_cv.wait(lock, [this] { return !audio_queue.empty() || !running; });
                
                if (!running) break;
                if (audio_queue.empty()) continue;
                
                samples = std::move(audio_queue.front());
                audio_queue.pop();
            }
            
            try {
                // Add samples to buffer
                buffer.insert(buffer.end(), samples.begin(), samples.end());
                
                // Process VAD in windows
                int current_offset = offset.load();
                while (current_offset + window_size < buffer.size()) {
                    vad->AcceptWaveform(buffer.data() + current_offset, window_size);
                    if (!speech_started.load() && vad->IsDetected()) {
                        speech_started = true;
                        started_time = std::chrono::steady_clock::now();
                        LOG_DEBUG(client_id, "Speech detected, starting recognition");
                    }
                    current_offset += window_size;
                    offset = current_offset;
                }
                
                // Clean up buffer when not speaking
                if (!speech_started.load()) {
                    if (buffer.size() > 10 * window_size) {
                        int new_offset = current_offset - (buffer.size() - 10 * window_size);
                        buffer = vector<float>(buffer.end() - 10 * window_size, buffer.end());
                        offset = new_offset;
                    }
                }
                
                // Periodic inference during speech (every 0.2 seconds)
                auto current_time = std::chrono::steady_clock::now();
                float elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - started_time).count() / 1000.0f;
                
                if (speech_started.load() && elapsed_seconds > 0.2) {
                    perform_recognition(false);
                    started_time = std::chrono::steady_clock::now();
                }
                
                // Process completed speech segments from VAD
                while (!vad->IsEmpty()) {
                    auto segment = vad->Front();
                    vad->Pop();
                    
                    process_speech_segment(segment);
                    
                    // Reset state after processing a complete segment
                    buffer.clear();
                    offset = 0;
                    speech_started = false;
                }
                
            } catch (const std::exception& e) {
                LOG_ERROR(client_id, "Error processing audio: " << e.what());
            }
        }
        
        LOG_INFO(client_id, "ASR session processing ended");
    }
    
    void perform_recognition(bool is_final) {
        try {
            OfflineStream stream = engine->get_recognizer().CreateStream();
            stream.AcceptWaveform(engine->get_sample_rate(), buffer.data(), buffer.size());
            engine->get_recognizer().Decode(&stream);
            
            OfflineRecognizerResult result = engine->get_recognizer().GetResult(&stream);
            string text = result.text;
            
            if (!text.empty()) {
                ASRResult asr_result;
                asr_result.text = result.text;
                asr_result.finished = is_final;
                asr_result.idx = segment_id.load();
                asr_result.lang = result.lang;
                asr_result.emotion = result.emotion;
                asr_result.event = result.event;
                asr_result.timestamps = result.timestamps;
                asr_result.tokens = result.tokens;
                
                send_result(asr_result);
                
                if (is_final) {
                    LOG_INFO(client_id, "Final result [" << segment_id.load() << "]: " << text);
                } else {
                    LOG_DEBUG(client_id, "Partial result [" << segment_id.load() << "]: " << text);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(client_id, "Error in recognition: " << e.what());
        }
    }
    
    void process_speech_segment(const SpeechSegment& segment) {
        try {
            OfflineStream stream = engine->get_recognizer().CreateStream();
            stream.AcceptWaveform(engine->get_sample_rate(), 
                                segment.samples.data(), segment.samples.size());
            engine->get_recognizer().Decode(&stream);
            
            OfflineRecognizerResult result = engine->get_recognizer().GetResult(&stream);
            string text = result.text;
            
            if (!text.empty()) {
                int current_segment_id = segment_id.fetch_add(1);
                processed_segments++;
                
                LOG_INFO(client_id, "Final result [" << current_segment_id << "]: " << text);
                
                ASRResult asr_result;
                asr_result.text = result.text;
                asr_result.finished = true;
                asr_result.idx = current_segment_id;
                asr_result.lang = result.lang;
                asr_result.emotion = result.emotion;
                asr_result.event = result.event;
                asr_result.timestamps = result.timestamps;
                asr_result.tokens = result.tokens;
                
                send_result(asr_result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR(client_id, "Error processing speech segment: " << e.what());
        }
    }
    
    void send_result(const ASRResult& result) {
        try {
            Json::Value json_result = result.to_json();
            Json::StreamWriterBuilder builder;
            string json_string = Json::writeString(builder, json_result);
            ws_server->send(hdl, json_string, websocketpp::frame::opcode::text);
            LOG_DEBUG(client_id, "Sent result: " << (result.finished ? "final" : "partial"));
        } catch (const std::exception& e) {
            LOG_ERROR(client_id, "Error sending result: " << e.what());
        }
    }
};

class WebSocketASRServer {
private:
    server ws_server;
    ASREngine asr_engine;
    ConnectionManager connection_manager;
    std::unordered_map<std::string, std::unique_ptr<ASRSession>> sessions;
    std::mutex sessions_mutex;
    string model_dir;
    int port;
    std::atomic<size_t> total_connections{0};
    std::atomic<size_t> active_sessions{0};
    
    // Performance monitoring
    std::thread monitor_thread;
    std::atomic<bool> monitoring{false};
    
public:
    WebSocketASRServer(const string& models_path, int server_port) 
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
    
    ~WebSocketASRServer() {
        stop_monitoring();
    }
    
    bool initialize() {
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
    
    void run() {
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
    
    void stop() {
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
    
private:
    void start_monitoring() {
        monitoring = true;
        monitor_thread = std::thread(&WebSocketASRServer::monitor_performance, this);
        LOG_INFO("SERVER", "Performance monitoring started");
    }
    
    void stop_monitoring() {
        if (monitoring.exchange(false)) {
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
            LOG_INFO("SERVER", "Performance monitoring stopped");
        }
    }
    
    void monitor_performance() {
        while (monitoring) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            if (!monitoring) break;
            
            size_t connections = connection_manager.get_connection_count();
            size_t sessions_count = active_sessions.load();
            
            LOG_INFO("SERVER", "Performance stats - Total connections: " << total_connections.load() 
                     << ", Active connections: " << connections 
                     << ", Active sessions: " << sessions_count);
            
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
    
    void on_open(connection_hdl hdl) {
        string client_id = connection_manager.add_connection(hdl);
        total_connections++;
        
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto session = std::make_unique<ASRSession>(&asr_engine, hdl, &ws_server, client_id);
        session->start();
        sessions[client_id] = std::move(session);
        active_sessions++;
        
        LOG_INFO(client_id, "New WebSocket connection opened. Total connections: " 
                 << connection_manager.get_connection_count());
    }
    
    void on_close(connection_hdl hdl) {
        string client_id = connection_manager.get_client_id(hdl);
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
    
    void on_message(connection_hdl hdl, message_ptr msg) {
        string client_id = connection_manager.get_client_id(hdl);
        
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(client_id);
        if (it != sessions.end() && it->second) {
            if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                const string& payload = msg->get_payload();
                vector<uint8_t> pcm_data(payload.begin(), payload.end());
                it->second->add_audio_data(pcm_data);
                LOG_DEBUG(client_id, "Received " << pcm_data.size() << " bytes of audio data");
            } else {
                LOG_WARN(client_id, "Received non-binary message, ignoring");
            }
        } else {
            LOG_WARN(client_id, "Received message for unknown session");
        }
    }
};

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
