#pragma once

#include "model_pool.h"
#include <sherpa-onnx/c-api/cxx-api.h>
#include <string>
#include <memory>
#include <atomic>

// 前向声明
class ServerConfig;

// 保持接口兼容性的包装器类
class ASREngine {
private:
    std::unique_ptr<ModelManager> model_manager;        // 向后兼容的旧接口
    std::unique_ptr<ModelPoolManager> pool_manager;     // 新的优化管理器
    std::atomic<bool> initialized;
    
public:
    ASREngine();
    ~ASREngine();
    
    bool initialize(const std::string& model_dir, const ServerConfig& config);
    
    bool is_initialized() const;
    float get_sample_rate() const;
    
    // Create a new VAD instance for each session
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad() const;
    
    // 新增方法：获取共享ASR引擎
    SharedASREngine* get_shared_asr() const;
    
    // 新增方法：获取VAD池
    VADPool* get_vad_pool() const;
    
    // 新增方法：释放VAD实例
    void release_vad(std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad) const;
    
    // 获取模型管理器的直接访问（用于高级用法）
    ModelManager* get_model_manager() const;
    
    // 兼容性接口 - 不推荐直接使用，应该使用模型池
    sherpa_onnx::cxx::OfflineRecognizer& get_recognizer() = delete;
};
