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
    std::unique_ptr<ModelManager> model_manager;
    std::atomic<bool> initialized;
    
public:
    ASREngine();
    ~ASREngine();
    
    bool initialize(const std::string& model_dir, const ServerConfig& config);
    
    bool is_initialized() const;
    float get_sample_rate() const;
    
    // Create a new VAD instance for each session
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad() const;
    
    // 获取模型管理器的直接访问（用于高级用法）
    ModelManager* get_model_manager() const;
    
    // 兼容性接口 - 不推荐直接使用，应该使用模型池
    sherpa_onnx::cxx::OfflineRecognizer& get_recognizer() = delete;
};
