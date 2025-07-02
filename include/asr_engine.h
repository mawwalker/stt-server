#pragma once

#include <sherpa-onnx/c-api/cxx-api.h>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

class ASREngine {
private:
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recognizer;
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad_template;
    float sample_rate;
    std::atomic<bool> initialized;
    mutable std::mutex engine_mutex;
    std::string model_directory;
    
public:
    ASREngine();
    
    bool initialize(const std::string& model_dir, int num_threads = 2);
    
    bool is_initialized() const;
    float get_sample_rate() const;
    
    // Create a new VAD instance for each session
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad() const;
    
    sherpa_onnx::cxx::OfflineRecognizer& get_recognizer();
};
