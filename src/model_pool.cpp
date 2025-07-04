#include "model_pool.h"
#include "server_config.h"
#include "logger.h"
#include <chrono>
#include <exception>

using namespace sherpa_onnx::cxx;

// ASRModelPool 实现
ASRModelPool::ASRModelPool(int pool_size) : total_instances(pool_size) {}

ASRModelPool::~ASRModelPool() {
    // 清理资源
    std::lock_guard<std::mutex> lock(pool_mutex);
    asr_instances.clear();
    while (!available_instances.empty()) {
        available_instances.pop();
    }
}

bool ASRModelPool::initialize(const std::string& model_dir, const ServerConfig& config) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    if (initialized.load()) {
        LOG_WARN("ASR_POOL", "ASR model pool already initialized");
        return true;
    }
    
    model_directory = model_dir;
    const auto& asr_config = config.get_asr_config();
    
    try {
        // 创建多个ASR实例
        for (int i = 0; i < total_instances; ++i) {
            auto instance = std::make_unique<ASRInstance>();
            instance->id = i;
            
            // 配置ASR
            OfflineRecognizerConfig recognizer_config;
            recognizer_config.model_config.sense_voice.model = 
                model_dir + "/" + asr_config.model_name + "/model.onnx";
            recognizer_config.model_config.sense_voice.use_itn = asr_config.use_itn;
            recognizer_config.model_config.sense_voice.language = asr_config.language;
            recognizer_config.model_config.tokens = 
                model_dir + "/" + asr_config.model_name + "/tokens.txt";
            
            // 每个实例使用较少的线程数，避免过度竞争
            recognizer_config.model_config.num_threads = std::max(1, asr_config.num_threads / total_instances);
            recognizer_config.model_config.debug = asr_config.debug;
            
            LOG_INFO("ASR_POOL", "Creating ASR instance " << i << " with " 
                     << recognizer_config.model_config.num_threads << " threads");
            
            auto recognizer_obj = OfflineRecognizer::Create(recognizer_config);
            if (!recognizer_obj.Get()) {
                LOG_ERROR("ASR_POOL", "Failed to create ASR instance " << i);
                return false;
            }
            
            instance->recognizer = std::make_unique<OfflineRecognizer>(std::move(recognizer_obj));
            
            asr_instances.push_back(std::move(instance));
            available_instances.push(i);
        }
        
        LOG_INFO("ASR_POOL", "ASR pool initialized with " << total_instances << " instances");
        initialized = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("ASR_POOL", "Error initializing ASR pool: " << e.what());
        return false;
    }
}

int ASRModelPool::acquire_recognizer(int timeout_ms) {
    std::unique_lock<std::mutex> lock(pool_mutex);
    
    if (!initialized.load()) {
        LOG_ERROR("ASR_POOL", "ASR pool not initialized");
        return -1;
    }
    
    // 等待可用实例
    auto timeout = std::chrono::milliseconds(timeout_ms);
    if (!pool_cv.wait_for(lock, timeout, [this] { return !available_instances.empty(); })) {
        LOG_WARN("ASR_POOL", "Timeout waiting for available ASR instance");
        return -1;
    }
    
    int instance_id = available_instances.front();
    available_instances.pop();
    
    asr_instances[instance_id]->in_use = true;
    
    LOG_DEBUG("ASR_POOL", "Acquired ASR instance " << instance_id 
              << ", available: " << available_instances.size());
    
    return instance_id;
}

void ASRModelPool::release_recognizer(int instance_id) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    if (instance_id < 0 || instance_id >= total_instances) {
        LOG_ERROR("ASR_POOL", "Invalid instance ID: " << instance_id);
        return;
    }
    
    if (!asr_instances[instance_id]->in_use.load()) {
        LOG_WARN("ASR_POOL", "Instance " << instance_id << " was not in use");
        return;
    }
    
    asr_instances[instance_id]->in_use = false;
    available_instances.push(instance_id);
    
    LOG_DEBUG("ASR_POOL", "Released ASR instance " << instance_id 
              << ", available: " << available_instances.size());
    
    pool_cv.notify_one();
}

OfflineRecognizer& ASRModelPool::get_recognizer(int instance_id) {
    if (instance_id < 0 || instance_id >= total_instances) {
        throw std::invalid_argument("Invalid instance ID");
    }
    
    if (!asr_instances[instance_id]->in_use.load()) {
        throw std::runtime_error("Instance not acquired");
    }
    
    return *asr_instances[instance_id]->recognizer;
}

float ASRModelPool::get_sample_rate() const {
    return sample_rate;
}

bool ASRModelPool::is_initialized() const {
    return initialized.load();
}

ASRModelPool::PoolStats ASRModelPool::get_pool_stats() const {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    PoolStats stats;
    stats.total_instances = total_instances;
    stats.available_instances = available_instances.size();
    stats.in_use_instances = total_instances - available_instances.size();
    
    return stats;
}

// VADModelPool 实现
VADModelPool::VADModelPool() {}

VADModelPool::~VADModelPool() {}

bool VADModelPool::initialize(const std::string& model_dir, const ServerConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex);
    
    if (initialized.load()) {
        LOG_WARN("VAD_POOL", "VAD model pool already initialized");
        return true;
    }
    
    model_directory = model_dir;
    const auto& vad_config_params = config.get_vad_config();
    window_size = vad_config_params.window_size;
    
    try {
        // 配置共享的VAD配置
        vad_config.silero_vad.model = model_dir + "/silero_vad/silero_vad.onnx";
        vad_config.silero_vad.threshold = vad_config_params.threshold;
        vad_config.silero_vad.min_silence_duration = vad_config_params.min_silence_duration;
        vad_config.silero_vad.min_speech_duration = vad_config_params.min_speech_duration;
        vad_config.silero_vad.max_speech_duration = vad_config_params.max_speech_duration;
        vad_config.sample_rate = vad_config_params.sample_rate;
        vad_config.debug = vad_config_params.debug;
        
        // 测试VAD配置是否有效
        auto test_vad = VoiceActivityDetector::Create(vad_config, window_size);
        if (!test_vad.Get()) {
            LOG_ERROR("VAD_POOL", "Failed to validate VAD configuration");
            return false;
        }
        
        LOG_INFO("VAD_POOL", "VAD pool initialized successfully");
        initialized = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("VAD_POOL", "Error initializing VAD pool: " << e.what());
        return false;
    }
}

std::unique_ptr<VoiceActivityDetector> VADModelPool::create_vad_instance() const {
    std::lock_guard<std::mutex> lock(config_mutex);
    
    if (!initialized.load()) {
        LOG_ERROR("VAD_POOL", "VAD pool not initialized");
        return nullptr;
    }
    
    try {
        // 为每个会话创建独立的VAD实例，但共享模型文件
        auto vad_obj = VoiceActivityDetector::Create(vad_config, window_size);
        if (!vad_obj.Get()) {
            LOG_ERROR("VAD_POOL", "Failed to create VAD instance");
            return nullptr;
        }
        
        return std::make_unique<VoiceActivityDetector>(std::move(vad_obj));
        
    } catch (const std::exception& e) {
        LOG_ERROR("VAD_POOL", "Error creating VAD instance: " << e.what());
        return nullptr;
    }
}

float VADModelPool::get_sample_rate() const {
    return sample_rate;
}

bool VADModelPool::is_initialized() const {
    return initialized.load();
}

// ModelManager 实现
ModelManager::ModelManager(int asr_pool_size) {
    asr_pool = std::make_unique<ASRModelPool>(asr_pool_size);
    vad_pool = std::make_unique<VADModelPool>();
}

ModelManager::~ModelManager() {}

bool ModelManager::initialize(const std::string& model_dir, const ServerConfig& config) {
    auto asr_stats = asr_pool->get_pool_stats();
    LOG_INFO("MODEL_MANAGER", "Initializing model manager with " 
             << asr_stats.total_instances << " ASR instances");
    
    // 初始化ASR池
    if (!asr_pool->initialize(model_dir, config)) {
        LOG_ERROR("MODEL_MANAGER", "Failed to initialize ASR pool");
        return false;
    }
    
    // 初始化VAD池
    if (!vad_pool->initialize(model_dir, config)) {
        LOG_ERROR("MODEL_MANAGER", "Failed to initialize VAD pool");
        return false;
    }
    
    initialized = true;
    LOG_INFO("MODEL_MANAGER", "Model manager initialized successfully");
    return true;
}

int ModelManager::acquire_asr_recognizer(int timeout_ms) {
    if (!initialized.load()) {
        LOG_ERROR("MODEL_MANAGER", "Model manager not initialized");
        return -1;
    }
    
    return asr_pool->acquire_recognizer(timeout_ms);
}

void ModelManager::release_asr_recognizer(int instance_id) {
    if (initialized.load()) {
        asr_pool->release_recognizer(instance_id);
    }
}

OfflineRecognizer& ModelManager::get_asr_recognizer(int instance_id) {
    if (!initialized.load()) {
        throw std::runtime_error("Model manager not initialized");
    }
    
    return asr_pool->get_recognizer(instance_id);
}

std::unique_ptr<VoiceActivityDetector> ModelManager::create_vad_instance() const {
    if (!initialized.load()) {
        LOG_ERROR("MODEL_MANAGER", "Model manager not initialized");
        return nullptr;
    }
    
    return vad_pool->create_vad_instance();
}

float ModelManager::get_sample_rate() const {
    return asr_pool->get_sample_rate();
}

bool ModelManager::is_initialized() const {
    return initialized.load();
}

ASRModelPool::PoolStats ModelManager::get_asr_pool_stats() const {
    if (initialized.load()) {
        return asr_pool->get_pool_stats();
    }
    
    return {0, 0, 0};
}
