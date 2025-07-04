#pragma once

#include <string>
#include <cstdlib>

// 运行环境类型
enum class RunEnvironment {
    LOCAL,      // 本地环境
    DOCKER,     // Docker容器环境
    AUTO        // 自动检测
};

// 服务器配置管理类
class ServerConfig {
public:
    struct ASRConfig {
        int pool_size = 2;                    // ASR模型池大小
        int num_threads = 2;                  // ASR线程数
        int acquire_timeout_ms = 5000;        // 获取ASR实例超时时间(ms)
        std::string model_name = "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17";
        bool use_itn = true;                  // 是否使用ITN(逆文本归一化)
        std::string language = "auto";        // 语言设置
        bool debug = false;                   // 调试模式
    };
    
    struct VADConfig {
        float threshold = 0.5f;               // VAD阈值
        float min_silence_duration = 0.25f;  // 最小静音时长(秒)
        float min_speech_duration = 0.25f;   // 最小语音时长(秒)
        float max_speech_duration = 8.0f;    // 最大语音时长(秒)
        float sample_rate = 16000.0f;         // 采样率
        int window_size = 100;                // VAD窗口大小
        bool debug = false;                   // 调试模式
    };
    
    struct VADPoolConfig {
        size_t min_pool_size = 2;             // VAD池最小大小
        size_t max_pool_size = 10;            // VAD池最大大小
        int acquire_timeout_ms = 5000;        // 获取VAD实例超时时间(ms)
    };
    
    struct ServerSettings {
        int port = 8000;                      // 服务器端口
        std::string models_root = "./assets"; // 模型根目录
        std::string log_level = "INFO";       // 日志级别
        int max_connections = 100;            // 最大连接数
        int connection_timeout_s = 300;       // 连接超时时间(秒)
    };
    
    struct PerformanceConfig {
        bool enable_memory_optimization = true;  // 启用内存优化
        size_t max_audio_buffer_size = 1024 * 1024; // 最大音频缓冲区大小(字节)
        int gc_interval_s = 60;               // 垃圾回收间隔(秒)
        bool enable_performance_logging = false; // 启用性能日志
    };

private:
    ASRConfig asr_config_;
    VADConfig vad_config_;
    VADPoolConfig vad_pool_config_;
    ServerSettings server_settings_;
    PerformanceConfig performance_config_;
    
    RunEnvironment run_env_ = RunEnvironment::AUTO;

    // 从环境变量读取整数值
    int get_env_int(const char* name, int default_value) const;
    
    // 从环境变量读取浮点数值
    float get_env_float(const char* name, float default_value) const;
    
    // 从环境变量读取字符串值
    std::string get_env_string(const char* name, const std::string& default_value) const;
    
    // 从环境变量读取布尔值
    bool get_env_bool(const char* name, bool default_value) const;

public:
    ServerConfig();
    
    // 从环境变量加载配置
    void load_from_environment();
    
    // 从命令行参数加载配置
    void load_from_args(int argc, char* argv[]);
    
    // 验证配置的有效性
    bool validate() const;
    
    // 打印当前配置
    void print_config() const;
    
    // 访问器
    const ASRConfig& get_asr_config() const { return asr_config_; }
    const VADConfig& get_vad_config() const { return vad_config_; }
    const VADPoolConfig& get_vad_pool_config() const { return vad_pool_config_; }
    const ServerSettings& get_server_settings() const { return server_settings_; }
    const PerformanceConfig& get_performance_config() const { return performance_config_; }
    
    // 修改器（用于命令行参数覆盖）
    ASRConfig& get_asr_config() { return asr_config_; }
    VADConfig& get_vad_config() { return vad_config_; }
    VADPoolConfig& get_vad_pool_config() { return vad_pool_config_; }
    ServerSettings& get_server_settings() { return server_settings_; }
    PerformanceConfig& get_performance_config() { return performance_config_; }
    
    // 静态方法：打印帮助信息
    static void print_usage(const char* program_name);

    // 环境检测和配置适配
    RunEnvironment detect_environment() const;
    void adapt_config_for_environment();
    std::string get_environment_name() const;

private:
    // 检查是否在Docker容器中运行
    bool is_running_in_docker() const;
};