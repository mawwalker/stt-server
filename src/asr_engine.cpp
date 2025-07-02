#include "asr_engine.h"
#include "logger.h"
#include <exception>

using namespace sherpa_onnx::cxx;

ASREngine::ASREngine() : sample_rate(16000), initialized(false) {}

bool ASREngine::initialize(const std::string& model_dir, int num_threads) {
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
    } catch (const std::exception& e) {
        LOG_ERROR("ENGINE", "Error initializing ASR engine: " << e.what());
        return false;
    }
}

bool ASREngine::is_initialized() const { 
    return initialized.load(); 
}

float ASREngine::get_sample_rate() const { 
    return sample_rate; 
}

std::unique_ptr<VoiceActivityDetector> ASREngine::create_vad() const {
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

OfflineRecognizer& ASREngine::get_recognizer() { 
    std::lock_guard<std::mutex> lock(engine_mutex);
    return *recognizer; 
}
