#pragma once

#include <sherpa-onnx/c-api/cxx-api.h>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <string>
#include <vector>
#include <chrono>

// 前向声明
class ServerConfig;

// VAD模型池管理器 - 管理VAD配置的共享和实例创建
class VADModelPool {
private:
    mutable std::mutex config_mutex;
    sherpa_onnx::cxx::VadModelConfig vad_config;
    std::string model_directory;
    float sample_rate = 16000;
    int window_size = 100;
    std::atomic<bool> initialized{false};
    
public:
    VADModelPool();
    ~VADModelPool();
    
    bool initialize(const std::string& model_dir, const ServerConfig& config);
    
    // 为每个会话创建独立的VAD实例
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad_instance() const;
    
    // 状态查询
    float get_sample_rate() const;
    bool is_initialized() const;
};

// VAD池管理器 - 管理VAD实例的复用
class VADPool {
private:
    std::queue<std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector>> vad_pool;
    std::mutex pool_mutex;
    std::condition_variable pool_cv;
    std::string model_directory;
    float sample_rate;
    std::atomic<size_t> total_instances{0};
    std::atomic<size_t> available_instances{0};
    const size_t max_instances;
    const size_t min_instances;
    sherpa_onnx::cxx::VadModelConfig vad_config;
    
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad_instance();
    
public:
    VADPool(const std::string& model_dir, const ServerConfig& config);
    ~VADPool();
    
    // 获取VAD实例（如果池为空会创建新实例或等待）
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // 归还VAD实例到池中
    void release(std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad);
    
    // 预热池 - 预先创建最小数量的实例
    bool initialize();
    
    // 获取池状态
    size_t get_total_instances() const { return total_instances.load(); }
    size_t get_available_instances() const { return available_instances.load(); }
    size_t get_active_instances() const { return total_instances.load() - available_instances.load(); }
};

// 共享ASR引擎管理器 - 管理单个ASR实例的线程安全访问
class SharedASREngine {
private:
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recognizer;
    mutable std::mutex engine_mutex;
    std::atomic<bool> initialized{false};
    std::string model_directory;
    float sample_rate;
    std::atomic<size_t> active_recognitions{0};
    
public:
    SharedASREngine();
    ~SharedASREngine();
    
    bool initialize(const std::string& model_dir, const ServerConfig& config);
    bool is_initialized() const { return initialized.load(); }
    float get_sample_rate() const { return sample_rate; }
    
    // 线程安全的识别接口
    std::string recognize(const float* samples, size_t sample_count);
    std::string recognize_with_metadata(const float* samples, size_t sample_count, 
                                      std::string& language, std::string& emotion, 
                                      std::string& event, std::vector<float>& timestamps);
    
    // 获取统计信息
    size_t get_active_recognitions() const { return active_recognitions.load(); }
};

// 模型池管理器 - 统一管理所有模型资源
class ModelPoolManager {
private:
    std::unique_ptr<SharedASREngine> asr_engine;
    std::unique_ptr<VADPool> vad_pool;
    mutable std::mutex stats_mutex;
    std::atomic<size_t> total_sessions{0};
    std::atomic<size_t> peak_concurrent_sessions{0};
    
public:
    ModelPoolManager();
    ~ModelPoolManager();
    
    // 初始化所有模型
    bool initialize(const std::string& model_dir, const ServerConfig& config);
    
    // 获取共享ASR引擎
    SharedASREngine* get_asr_engine() { return asr_engine.get(); }
    
    // 获取VAD池
    VADPool* get_vad_pool() { return vad_pool.get(); }
    
    // 会话生命周期管理
    void session_started();
    void session_ended();
    
    // 获取系统统计信息
    struct SystemStats {
        size_t total_sessions;
        size_t peak_concurrent_sessions;
        size_t current_active_sessions;
        size_t asr_active_recognitions;
        size_t vad_total_instances;
        size_t vad_available_instances;
        size_t vad_active_instances;
        float memory_efficiency_ratio; // active_instances / total_instances
    };
    
    SystemStats get_system_stats() const;
    void log_system_stats() const;
};

// 简化的模型管理器 - 仅用于向后兼容，建议使用 ModelPoolManager
class ModelManager {
private:
    std::unique_ptr<VADModelPool> vad_pool;
    std::atomic<bool> initialized{false};
    
    // 向后兼容：保留共享ASR引擎的引用
    std::unique_ptr<SharedASREngine> shared_asr;
    
public:
    explicit ModelManager(int asr_pool_size = 2); // 参数保留但不使用
    ~ModelManager();
    
    bool initialize(const std::string& model_dir, const ServerConfig& config);
    
    // ASR接口 - 重定向到共享引擎
    int acquire_asr_recognizer(int timeout_ms = 5000);
    void release_asr_recognizer(int instance_id);
    
    // VAD接口
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad_instance() const;
    
    // 状态查询
    float get_sample_rate() const;
    bool is_initialized() const;
    
    // 替代PoolStats的简化统计
    struct LegacyStats {
        size_t total_instances = 1;      // 共享ASR只有1个实例
        size_t available_instances = 1;  // 总是可用
        size_t in_use_instances = 0;     // 共享模式下无需跟踪
    };
    LegacyStats get_asr_pool_stats() const;
};
