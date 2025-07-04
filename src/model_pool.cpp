#include "model_pool.h"
#include "server_config.h"
#include "logger.h"
#include <chrono>
#include <exception>

using namespace sherpa_onnx::cxx;

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

// ModelManager 实现 - 向后兼容的接口
ModelManager::ModelManager([[maybe_unused]] int asr_pool_size) {
    // asr_pool_size 参数保留但不使用，因为现在使用共享ASR
    shared_asr = std::make_unique<SharedASREngine>();
    vad_pool = std::make_unique<VADModelPool>();
}

ModelManager::~ModelManager() {}

bool ModelManager::initialize(const std::string& model_dir, const ServerConfig& config) {
    LOG_INFO("MODEL_MANAGER", "Initializing legacy model manager (using shared ASR)");
    
    // 初始化共享ASR引擎
    if (!shared_asr->initialize(model_dir, config)) {
        LOG_ERROR("MODEL_MANAGER", "Failed to initialize shared ASR engine");
        return false;
    }
    
    // 初始化VAD池
    if (!vad_pool->initialize(model_dir, config)) {
        LOG_ERROR("MODEL_MANAGER", "Failed to initialize VAD pool");
        return false;
    }
    
    initialized = true;
    LOG_INFO("MODEL_MANAGER", "Legacy model manager initialized successfully");
    return true;
}

int ModelManager::acquire_asr_recognizer([[maybe_unused]] int timeout_ms) {
    if (!initialized.load()) {
        LOG_ERROR("MODEL_MANAGER", "Model manager not initialized");
        return -1;
    }
    
    // 在共享模式下，总是返回固定ID 0，表示可以使用共享引擎
    if (shared_asr && shared_asr->is_initialized()) {
        LOG_DEBUG("MODEL_MANAGER", "Acquired shared ASR engine (ID: 0)");
        return 0;
    }
    
    LOG_ERROR("MODEL_MANAGER", "Shared ASR engine not available");
    return -1;
}

void ModelManager::release_asr_recognizer(int instance_id) {
    // 在共享模式下，释放操作是空操作
    LOG_DEBUG("MODEL_MANAGER", "Released shared ASR engine (ID: " << instance_id << ")");
}

OfflineRecognizer& ModelManager::get_asr_recognizer([[maybe_unused]] int instance_id) {
    if (!initialized.load()) {
        throw std::runtime_error("Model manager not initialized");
    }
    
    if (!shared_asr || !shared_asr->is_initialized()) {
        throw std::runtime_error("Shared ASR engine not available");
    }
    
    // 注意：这个方法不应该在新代码中使用，因为它不是线程安全的
    // 新代码应该使用 SharedASREngine::recognize() 方法
    LOG_WARN("MODEL_MANAGER", "Using legacy get_asr_recognizer - consider switching to SharedASREngine::recognize()");
    
    // 这里我们不能直接返回共享引擎的recognizer，因为它不是线程安全的
    // 为了向后兼容，我们抛出异常提示用户使用新接口
    throw std::runtime_error("Legacy get_asr_recognizer() is deprecated. Use SharedASREngine::recognize() instead.");
}

std::unique_ptr<VoiceActivityDetector> ModelManager::create_vad_instance() const {
    if (!initialized.load()) {
        LOG_ERROR("MODEL_MANAGER", "Model manager not initialized");
        return nullptr;
    }
    
    return vad_pool->create_vad_instance();
}

float ModelManager::get_sample_rate() const {
    if (shared_asr) {
        return shared_asr->get_sample_rate();
    }
    return 16000; // 默认值
}

bool ModelManager::is_initialized() const {
    return initialized.load();
}

ModelManager::LegacyStats ModelManager::get_asr_pool_stats() const {
    LegacyStats stats;
    if (initialized.load()) {
        // 共享ASR模式下的统计信息
        stats.total_instances = 1;
        stats.available_instances = 1;
        stats.in_use_instances = shared_asr ? shared_asr->get_active_recognitions() : 0;
    }
    
    return stats;
}

// SharedASREngine 实现 - 共享ASR引擎，支持多客户端并发访问
SharedASREngine::SharedASREngine() {}

SharedASREngine::~SharedASREngine() {}

bool SharedASREngine::initialize(const std::string& model_dir, const ServerConfig& config) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    if (initialized.load()) {
        LOG_WARN("SHARED_ASR", "Shared ASR engine already initialized");
        return true;
    }
    
    model_directory = model_dir;
    const auto& asr_config = config.get_asr_config();
    
    try {
        // 配置共享ASR
        OfflineRecognizerConfig recognizer_config;
        recognizer_config.model_config.sense_voice.model = 
            model_dir + "/" + asr_config.model_name + "/model.onnx";
        recognizer_config.model_config.sense_voice.use_itn = asr_config.use_itn;
        recognizer_config.model_config.sense_voice.language = asr_config.language;
        recognizer_config.model_config.tokens = 
            model_dir + "/" + asr_config.model_name + "/tokens.txt";
        
        // 使用所有可用线程，因为是共享的
        recognizer_config.model_config.num_threads = asr_config.num_threads;
        recognizer_config.model_config.debug = asr_config.debug;
        
        LOG_INFO("SHARED_ASR", "Creating shared ASR engine with " 
                 << recognizer_config.model_config.num_threads << " threads");
        
        auto recognizer_obj = OfflineRecognizer::Create(recognizer_config);
        if (!recognizer_obj.Get()) {
            LOG_ERROR("SHARED_ASR", "Failed to create shared ASR engine");
            return false;
        }
        
        recognizer = std::make_unique<OfflineRecognizer>(std::move(recognizer_obj));
        sample_rate = 16000; // 设置采样率
        
        LOG_INFO("SHARED_ASR", "Shared ASR engine initialized successfully");
        initialized = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("SHARED_ASR", "Error initializing shared ASR engine: " << e.what());
        return false;
    }
}

std::string SharedASREngine::recognize(const float* samples, size_t sample_count) {
    if (!initialized.load()) {
        LOG_ERROR("SHARED_ASR", "Shared ASR engine not initialized");
        return "";
    }
    
    // 线程安全的识别
    std::lock_guard<std::mutex> lock(engine_mutex);
    active_recognitions++;
    
    try {
        OfflineStream stream = recognizer->CreateStream();
        stream.AcceptWaveform(sample_rate, samples, sample_count);
        recognizer->Decode(&stream);
        
        OfflineRecognizerResult result = recognizer->GetResult(&stream);
        active_recognitions--;
        
        return result.text;
        
    } catch (const std::exception& e) {
        active_recognitions--;
        LOG_ERROR("SHARED_ASR", "Error in recognition: " << e.what());
        return "";
    }
}

std::string SharedASREngine::recognize_with_metadata(const float* samples, size_t sample_count, 
                                                   std::string& language, std::string& emotion, 
                                                   std::string& event, std::vector<float>& timestamps) {
    if (!initialized.load()) {
        LOG_ERROR("SHARED_ASR", "Shared ASR engine not initialized");
        return "";
    }
    
    // 线程安全的识别
    std::lock_guard<std::mutex> lock(engine_mutex);
    active_recognitions++;
    
    try {
        OfflineStream stream = recognizer->CreateStream();
        stream.AcceptWaveform(sample_rate, samples, sample_count);
        recognizer->Decode(&stream);
        
        OfflineRecognizerResult result = recognizer->GetResult(&stream);
        active_recognitions--;
        
        language = result.lang;
        emotion = result.emotion;
        event = result.event;
        timestamps = result.timestamps;
        
        return result.text;
        
    } catch (const std::exception& e) {
        active_recognitions--;
        LOG_ERROR("SHARED_ASR", "Error in recognition with metadata: " << e.what());
        return "";
    }
}

// VADPool 实现 - 动态VAD池管理
VADPool::VADPool(const std::string& model_dir, const ServerConfig& config) 
    : model_directory(model_dir), max_instances(20), min_instances(2) {
    
    const auto& vad_config_params = config.get_vad_config();
    
    // 配置VAD
    vad_config.silero_vad.model = model_dir + "/silero_vad/silero_vad.onnx";
    vad_config.silero_vad.threshold = vad_config_params.threshold;
    vad_config.silero_vad.min_silence_duration = vad_config_params.min_silence_duration;
    vad_config.silero_vad.min_speech_duration = vad_config_params.min_speech_duration;
    vad_config.silero_vad.max_speech_duration = vad_config_params.max_speech_duration;
    vad_config.sample_rate = vad_config_params.sample_rate;
    sample_rate = vad_config_params.sample_rate;
}

VADPool::~VADPool() {
    std::lock_guard<std::mutex> lock(pool_mutex);
    while (!vad_pool.empty()) {
        vad_pool.pop();
    }
}

std::unique_ptr<VoiceActivityDetector> VADPool::create_vad_instance() {
    try {
        auto vad_obj = VoiceActivityDetector::Create(vad_config, 100); // window_size = 100
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

std::unique_ptr<VoiceActivityDetector> VADPool::acquire(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(pool_mutex);
    
    // 如果池中有可用实例，直接返回
    if (!vad_pool.empty()) {
        auto vad = std::move(vad_pool.front());
        vad_pool.pop();
        available_instances--;
        LOG_DEBUG("VAD_POOL", "Acquired VAD from pool, available: " << available_instances.load());
        return vad;
    }
    
    // 如果没达到最大实例数，创建新实例
    if (total_instances.load() < max_instances) {
        lock.unlock();
        auto vad = create_vad_instance();
        if (vad) {
            total_instances++;
            LOG_DEBUG("VAD_POOL", "Created new VAD instance, total: " << total_instances.load());
            return vad;
        }
        lock.lock();
    }
    
    // 等待可用实例
    if (pool_cv.wait_for(lock, timeout, [this] { return !vad_pool.empty(); })) {
        auto vad = std::move(vad_pool.front());
        vad_pool.pop();
        available_instances--;
        LOG_DEBUG("VAD_POOL", "Acquired VAD after wait, available: " << available_instances.load());
        return vad;
    }
    
    LOG_WARN("VAD_POOL", "Timeout waiting for available VAD instance");
    return nullptr;
}

void VADPool::release(std::unique_ptr<VoiceActivityDetector> vad) {
    if (!vad) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    // 如果池满了，直接丢弃实例
    if (vad_pool.size() >= max_instances) {
        total_instances--;
        LOG_DEBUG("VAD_POOL", "Pool full, discarding VAD instance, total: " << total_instances.load());
        return;
    }
    
    vad_pool.push(std::move(vad));
    available_instances++;
    LOG_DEBUG("VAD_POOL", "Released VAD to pool, available: " << available_instances.load());
    
    pool_cv.notify_one();
}

bool VADPool::initialize() {
    // 预热池 - 创建最小数量的实例
    for (size_t i = 0; i < min_instances; ++i) {
        auto vad = create_vad_instance();
        if (!vad) {
            LOG_ERROR("VAD_POOL", "Failed to create initial VAD instance " << i);
            return false;
        }
        
        std::lock_guard<std::mutex> lock(pool_mutex);
        vad_pool.push(std::move(vad));
        total_instances++;
        available_instances++;
    }
    
    LOG_INFO("VAD_POOL", "VAD pool initialized with " << min_instances << " instances");
    return true;
}

// ModelPoolManager 实现 - 统一管理ASR和VAD资源
ModelPoolManager::ModelPoolManager() {
    asr_engine = std::make_unique<SharedASREngine>();
}

ModelPoolManager::~ModelPoolManager() {}

bool ModelPoolManager::initialize(const std::string& model_dir, const ServerConfig& config) {
    LOG_INFO("MODEL_POOL_MANAGER", "Initializing model pool manager");
    
    // 初始化共享ASR引擎
    if (!asr_engine->initialize(model_dir, config)) {
        LOG_ERROR("MODEL_POOL_MANAGER", "Failed to initialize shared ASR engine");
        return false;
    }
    
    // 初始化VAD池
    vad_pool = std::make_unique<VADPool>(model_dir, config);
    if (!vad_pool->initialize()) {
        LOG_ERROR("MODEL_POOL_MANAGER", "Failed to initialize VAD pool");
        return false;
    }
    
    LOG_INFO("MODEL_POOL_MANAGER", "Model pool manager initialized successfully");
    return true;
}

void ModelPoolManager::session_started() {
    auto current = total_sessions.fetch_add(1) + 1;
    
    // 更新峰值并发会话数
    auto peak = peak_concurrent_sessions.load();
    while (current > peak && !peak_concurrent_sessions.compare_exchange_weak(peak, current)) {
        peak = peak_concurrent_sessions.load();
    }
    
    LOG_INFO("MODEL_POOL_MANAGER", "Session started, current active: " << current);
}

void ModelPoolManager::session_ended() {
    auto current = total_sessions.fetch_sub(1) - 1;
    LOG_INFO("MODEL_POOL_MANAGER", "Session ended, current active: " << current);
}

ModelPoolManager::SystemStats ModelPoolManager::get_system_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex);
    
    SystemStats stats;
    stats.total_sessions = total_sessions.load();
    stats.peak_concurrent_sessions = peak_concurrent_sessions.load();
    stats.current_active_sessions = total_sessions.load();
    stats.asr_active_recognitions = asr_engine->get_active_recognitions();
    stats.vad_total_instances = vad_pool->get_total_instances();
    stats.vad_available_instances = vad_pool->get_available_instances();
    stats.vad_active_instances = vad_pool->get_active_instances();
    
    if (stats.vad_total_instances > 0) {
        stats.memory_efficiency_ratio = static_cast<float>(stats.vad_active_instances) / stats.vad_total_instances;
    } else {
        stats.memory_efficiency_ratio = 0.0f;
    }
    
    return stats;
}

void ModelPoolManager::log_system_stats() const {
    auto stats = get_system_stats();
    LOG_INFO("MODEL_POOL_MANAGER", 
             "System stats - Active sessions: " << stats.current_active_sessions
             << ", Peak sessions: " << stats.peak_concurrent_sessions
             << ", ASR recognitions: " << stats.asr_active_recognitions
             << ", VAD instances (total/available/active): " 
             << stats.vad_total_instances << "/" 
             << stats.vad_available_instances << "/" 
             << stats.vad_active_instances
             << ", Memory efficiency: " << (stats.memory_efficiency_ratio * 100) << "%");
}
