#include "asr_engine.h"
#include "logger.h"
#include <exception>

ASREngine::ASREngine() : initialized(false) {}

ASREngine::~ASREngine() {}

bool ASREngine::initialize(const std::string& model_dir, int num_threads) {
    if (initialized.load()) {
        LOG_WARN("ENGINE", "ASR engine already initialized");
        return true;
    }
    
    try {
        // 根据并发需求动态调整池大小
        int pool_size = std::max(2, num_threads); // 至少2个实例
        
        model_manager = std::make_unique<ModelManager>(pool_size);
        
        if (!model_manager->initialize(model_dir, num_threads)) {
            LOG_ERROR("ENGINE", "Failed to initialize model manager");
            return false;
        }
        
        initialized = true;
        LOG_INFO("ENGINE", "ASR engine initialized with pool size: " << pool_size);
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
    if (!initialized.load() || !model_manager) {
        return 16000; // 默认值
    }
    return model_manager->get_sample_rate();
}

std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> ASREngine::create_vad() const {
    if (!initialized.load() || !model_manager) {
        LOG_ERROR("ENGINE", "ASR engine not initialized");
        return nullptr;
    }
    
    return model_manager->create_vad_instance();
}

ModelManager* ASREngine::get_model_manager() const {
    return model_manager.get();
}
