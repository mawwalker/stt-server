#include "oneshot_asr_session.h"
#include "logger.h"
#include <json/json.h>
#include <cstdint>
#include <algorithm>

OneShotASRSession::OneShotASRSession(ASREngine* eng, connection_hdl h, server* srv, const std::string& id) 
    : engine(eng), hdl(h), ws_server(srv), client_id(id), running(true), recording(false),
      state(SessionState::WAITING_START), session_start_time(std::chrono::steady_clock::now()) {
}

OneShotASRSession::~OneShotASRSession() {
    stop();
    auto session_duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - session_start_time).count();
    LOG_INFO(client_id, "OneShot session ended. Duration: " << session_duration << "s");
}

void OneShotASRSession::start() {
    LOG_INFO(client_id, "Starting OneShot ASR session");
    state = SessionState::WAITING_START;
    send_status("ready");
}

void OneShotASRSession::stop() {
    if (running.exchange(false)) {
        LOG_INFO(client_id, "Stopping OneShot ASR session");
        recording = false;
        state = SessionState::FINISHED;
    }
}

void OneShotASRSession::handle_message(const std::string& message) {
    if (!running) return;
    
    try {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(message, root)) {
            send_error("Invalid JSON message");
            return;
        }
        
        std::string command = root.get("command", "").asString();
        LOG_DEBUG(client_id, "Received command: " << command);
        
        if (command == "start") {
            if (state == SessionState::WAITING_START) {
                start_recording();
            } else {
                send_error("Invalid state for start command");
            }
        } else if (command == "stop") {
            if (state == SessionState::RECORDING) {
                stop_recording_and_process();
            } else {
                send_error("Invalid state for stop command");
            }
        } else {
            send_error("Unknown command: " + command);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(client_id, "Error handling message: " << e.what());
        send_error("Error processing message");
    }
}

void OneShotASRSession::add_audio_data(const std::vector<uint8_t>& pcm_bytes) {
    if (!running || !recording || state != SessionState::RECORDING) return;
    
    // Convert PCM bytes to float samples (assuming 16-bit PCM)
    std::lock_guard<std::mutex> lock(audio_mutex);
    size_t num_samples = pcm_bytes.size() / 2;
    
    for (size_t i = 0; i < num_samples; i++) {
        int16_t sample = static_cast<int16_t>(pcm_bytes[i * 2] | (pcm_bytes[i * 2 + 1] << 8));
        float normalized_sample = static_cast<float>(sample) / 32768.0f;
        audio_buffer.push_back(normalized_sample);
    }
    
    LOG_DEBUG(client_id, "Added " << num_samples << " audio samples, total: " << audio_buffer.size());
}

std::string OneShotASRSession::get_client_id() const {
    return client_id;
}

bool OneShotASRSession::is_running() const {
    return running;
}

bool OneShotASRSession::is_recording() const {
    return recording;
}

void OneShotASRSession::start_recording() {
    LOG_INFO(client_id, "Starting audio recording");
    std::lock_guard<std::mutex> lock(audio_mutex);
    audio_buffer.clear();
    recording = true;
    state = SessionState::RECORDING;
    recording_start_time = std::chrono::steady_clock::now();
    send_status("recording");
}

void OneShotASRSession::stop_recording_and_process() {
    LOG_INFO(client_id, "Stopping recording and starting processing");
    recording = false;
    state = SessionState::PROCESSING;
    
    auto recording_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - recording_start_time).count();
    LOG_INFO(client_id, "Recording duration: " << recording_duration << "ms");
    
    send_status("processing");
    process_complete_audio();
}

void OneShotASRSession::process_complete_audio() {
    std::lock_guard<std::mutex> lock(audio_mutex);
    
    if (audio_buffer.empty()) {
        LOG_WARN(client_id, "No audio data to process");
        send_error("No audio data received");
        return;
    }
    
    LOG_INFO(client_id, "Processing " << audio_buffer.size() << " audio samples");
    
    try {
        // 获取共享ASR引擎进行识别
        auto shared_asr = engine->get_shared_asr();
        if (!shared_asr || !shared_asr->is_initialized()) {
            send_error("Shared ASR engine not available");
            return;
        }
        
        // 使用共享ASR引擎进行识别，支持元数据
        std::string language, emotion, event;
        std::vector<float> timestamps;
        
        std::string text = shared_asr->recognize_with_metadata(
            audio_buffer.data(), 
            audio_buffer.size(),
            language,
            emotion, 
            event,
            timestamps
        );
        
        if (text.empty()) {
            send_error("Recognition failed - no result");
            return;
        }
        
        // 构造结果
        ASRResult asr_result;
        asr_result.text = text;
        asr_result.finished = true;
        asr_result.idx = 0;
        asr_result.lang = language.empty() ? "auto" : language;
        asr_result.emotion = emotion;
        asr_result.event = event;
        
        // 添加时间戳信息
        if (!timestamps.empty()) {
            asr_result.timestamps = timestamps;
        }
        
        LOG_INFO(client_id, "Recognition completed: " << asr_result.text);
        send_result(asr_result);
        
        state = SessionState::FINISHED;
        send_status("finished");
        
    } catch (const std::exception& e) {
        LOG_ERROR(client_id, "Error during recognition: " << e.what());
        send_error("Recognition failed: " + std::string(e.what()));
    }
}

void OneShotASRSession::send_result(const ASRResult& result) {
    try {
        Json::Value json_result = result.to_json();
        json_result["type"] = "result";
        
        Json::StreamWriterBuilder builder;
        std::string json_string = Json::writeString(builder, json_result);
        ws_server->send(hdl, json_string, websocketpp::frame::opcode::text);
        
        LOG_DEBUG(client_id, "Sent result: " << result.text);
    } catch (const std::exception& e) {
        LOG_ERROR(client_id, "Error sending result: " << e.what());
    }
}

void OneShotASRSession::send_error(const std::string& error_message) {
    try {
        Json::Value error_json;
        error_json["type"] = "error";
        error_json["message"] = error_message;
        
        Json::StreamWriterBuilder builder;
        std::string json_string = Json::writeString(builder, error_json);
        ws_server->send(hdl, json_string, websocketpp::frame::opcode::text);
        
        LOG_ERROR(client_id, "Sent error: " << error_message);
    } catch (const std::exception& e) {
        LOG_ERROR(client_id, "Error sending error message: " << e.what());
    }
}

void OneShotASRSession::send_status(const std::string& status) {
    try {
        Json::Value status_json;
        status_json["type"] = "status";
        status_json["status"] = status;
        
        Json::StreamWriterBuilder builder;
        std::string json_string = Json::writeString(builder, status_json);
        ws_server->send(hdl, json_string, websocketpp::frame::opcode::text);
        
        LOG_DEBUG(client_id, "Sent status: " << status);
    } catch (const std::exception& e) {
        LOG_ERROR(client_id, "Error sending status: " << e.what());
    }
}
