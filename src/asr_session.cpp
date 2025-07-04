#include "asr_session.h"
#include "logger.h"
#include <json/json.h>
#include <cstdint>

using namespace sherpa_onnx::cxx;

ASRSession::ASRSession(ASREngine* eng, connection_hdl h, server* srv, const std::string& id) 
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

ASRSession::~ASRSession() {
    stop();
    auto session_duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - session_start_time).count();
    LOG_INFO(client_id, "Session ended. Duration: " << session_duration 
             << "s, Processed samples: " << processed_samples.load() 
             << ", Segments: " << processed_segments.load());
}

void ASRSession::start() {
    if (!vad || !running) {
        LOG_ERROR(client_id, "Cannot start session - VAD not available");
        return;
    }
    LOG_INFO(client_id, "Starting ASR session");
    processing_thread = std::thread(&ASRSession::process_audio, this);
}

void ASRSession::stop() {
    if (running.exchange(false)) {
        LOG_INFO(client_id, "Stopping ASR session");
        audio_cv.notify_all();
        if (processing_thread.joinable()) {
            processing_thread.join();
        }
    }
}

void ASRSession::add_audio_data(const std::vector<uint8_t>& pcm_bytes) {
    if (!running) return;
    
    // Convert PCM bytes to float samples
    std::vector<float> samples;
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

std::string ASRSession::get_client_id() const { 
    return client_id; 
}

bool ASRSession::is_running() const { 
    return running.load(); 
}

void ASRSession::process_audio() {
    if (!vad) {
        LOG_ERROR(client_id, "VAD not available for processing");
        return;
    }
    
    LOG_INFO(client_id, "ASR session processing started");
    
    while (running) {
        std::vector<float> samples;
        
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
            while (current_offset + window_size < static_cast<int>(buffer.size())) {
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
                    buffer = std::vector<float>(buffer.end() - 10 * window_size, buffer.end());
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

void ASRSession::perform_recognition(bool is_final) {
    ModelManager* model_manager = engine->get_model_manager();
    if (!model_manager) {
        LOG_ERROR(client_id, "Model manager not available");
        return;
    }
    
    // 获取ASR实例
    int asr_instance_id = model_manager->acquire_asr_recognizer(3000); // 3秒超时
    if (asr_instance_id < 0) {
        LOG_WARN(client_id, "Failed to acquire ASR recognizer, skipping recognition");
        return;
    }
    
    try {
        auto& recognizer = model_manager->get_asr_recognizer(asr_instance_id);
        
        OfflineStream stream = recognizer.CreateStream();
        stream.AcceptWaveform(engine->get_sample_rate(), buffer.data(), buffer.size());
        recognizer.Decode(&stream);
        
        OfflineRecognizerResult result = recognizer.GetResult(&stream);
        std::string text = result.text;
        
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
    
    // 释放ASR实例
    model_manager->release_asr_recognizer(asr_instance_id);
}

void ASRSession::process_speech_segment(const SpeechSegment& segment) {
    ModelManager* model_manager = engine->get_model_manager();
    if (!model_manager) {
        LOG_ERROR(client_id, "Model manager not available");
        return;
    }
    
    // 获取ASR实例
    int asr_instance_id = model_manager->acquire_asr_recognizer(3000); // 3秒超时
    if (asr_instance_id < 0) {
        LOG_WARN(client_id, "Failed to acquire ASR recognizer for speech segment");
        return;
    }
    
    try {
        auto& recognizer = model_manager->get_asr_recognizer(asr_instance_id);
        
        OfflineStream stream = recognizer.CreateStream();
        stream.AcceptWaveform(engine->get_sample_rate(), 
                            segment.samples.data(), segment.samples.size());
        recognizer.Decode(&stream);
        
        OfflineRecognizerResult result = recognizer.GetResult(&stream);
        std::string text = result.text;
        
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
    
    // 释放ASR实例
    model_manager->release_asr_recognizer(asr_instance_id);
}

void ASRSession::send_result(const ASRResult& result) {
    try {
        Json::Value json_result = result.to_json();
        Json::StreamWriterBuilder builder;
        std::string json_string = Json::writeString(builder, json_result);
        ws_server->send(hdl, json_string, websocketpp::frame::opcode::text);
        LOG_DEBUG(client_id, "Sent result: " << (result.finished ? "final" : "partial"));
    } catch (const std::exception& e) {
        LOG_ERROR(client_id, "Error sending result: " << e.what());
    }
}
