#include "asr_engine.h"
#include "server_config.h"
#include "logger.h"
#include <exception>

ASREngine::ASREngine() : initialized(false) {}

ASREngine::~ASREngine() {}

bool ASREngine::initialize(const std::string& model_dir, const ServerConfig& config) {
    if (initialized.load()) {
        LOG_WARN("ENGINE", "ASR engine already initialized");
        return true;
    }
    
    try {
        // 使用新的ModelPoolManager而不是旧的ModelManager
        pool_manager = std::make_unique<ModelPoolManager>();
        
        if (!pool_manager->initialize(model_dir, config)) {
            LOG_ERROR("ENGINE", "Failed to initialize model pool manager");
            return false;
        }
        
        // 保留向后兼容性，仍然创建ModelManager以供旧代码使用
        const auto& asr_config = config.get_asr_config();
        model_manager = std::make_unique<ModelManager>(asr_config.pool_size);
        if (!model_manager->initialize(model_dir, config)) {
            LOG_ERROR("ENGINE", "Failed to initialize legacy model manager");
            return false;
        }
        
        initialized = true;
        LOG_INFO("ENGINE", "ASR engine initialized with shared ASR model and dynamic VAD pool");
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
    if (!initialized.load()) {
        LOG_ERROR("ENGINE", "ASR engine not initialized");
        return nullptr;
    }
    
    // 优先使用新的pool_manager
    if (pool_manager) {
        auto vad = pool_manager->get_vad_pool()->acquire(std::chrono::milliseconds(3000));
        if (!vad) {
            LOG_WARN("ENGINE", "Failed to acquire VAD from pool, falling back to legacy creation");
        } else {
            return vad;
        }
    }
    
    // 向后兼容：使用旧的model_manager
    if (model_manager) {
        return model_manager->create_vad_instance();
    }
    
    LOG_ERROR("ENGINE", "No VAD creation method available");
    return nullptr;
}

// 新增方法：获取共享ASR引擎
SharedASREngine* ASREngine::get_shared_asr() const {
    if (!initialized.load() || !pool_manager) {
        return nullptr;
    }
    return pool_manager->get_asr_engine();
}

// 新增方法：获取VAD池
VADPool* ASREngine::get_vad_pool() const {
    if (!initialized.load() || !pool_manager) {
        return nullptr;
    }
    return pool_manager->get_vad_pool();
}

// 新增方法：释放VAD实例
void ASREngine::release_vad(std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad) const {
    if (pool_manager) {
        pool_manager->get_vad_pool()->release(std::move(vad));
    }
}

ModelManager* ASREngine::get_model_manager() const {
    return model_manager.get();
}
