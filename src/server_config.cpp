#include "server_config.h"
#include "logger.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <fstream>

ServerConfig::ServerConfig() {
    // 构造函数中设置默认值（已在头文件中设置）
    // 检测运行环境并适配配置
    run_env_ = detect_environment();
    adapt_config_for_environment();
}

RunEnvironment ServerConfig::detect_environment() const {
    // 首先检查环境变量RUN_ENVIRONMENT
    const char* env_var = std::getenv("RUN_ENVIRONMENT");
    if (env_var != nullptr) {
        std::string env_str(env_var);
        std::transform(env_str.begin(), env_str.end(), env_str.begin(), ::tolower);
        
        if (env_str == "docker") {
            return RunEnvironment::DOCKER;
        } else if (env_str == "local") {
            return RunEnvironment::LOCAL;
        }
    }
    
    // 自动检测：检查是否在Docker容器中
    if (is_running_in_docker()) {
        return RunEnvironment::DOCKER;
    }
    
    return RunEnvironment::LOCAL;
}

bool ServerConfig::is_running_in_docker() const {
    // 方法1: 检查/.dockerenv文件
    std::ifstream dockerenv("/.dockerenv");
    if (dockerenv.good()) {
        return true;
    }
    
    // 方法2: 检查/proc/1/cgroup是否包含docker
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("docker") != std::string::npos || 
                line.find("containerd") != std::string::npos) {
                return true;
            }
        }
    }
    
    // 方法3: 检查容器相关环境变量
    const char* container_env = std::getenv("CONTAINER");
    if (container_env && std::string(container_env) == "docker") {
        return true;
    }
    
    return false;
}

void ServerConfig::adapt_config_for_environment() {
    if (run_env_ == RunEnvironment::DOCKER) {
        // Docker环境配置适配
        LOG_INFO("CONFIG", "Detected Docker environment, adapting configuration");
        
        // 如果MODELS_ROOT未设置，设置为Docker路径
        if (server_settings_.models_root == "./assets") {
            server_settings_.models_root = "/app/assets";
        }
        
        // Docker环境下的性能优化
        // 可以根据容器资源限制调整配置
        
    } else {
        // 本地环境配置适配
        LOG_INFO("CONFIG", "Detected local environment, using local configuration");
        
        // 如果MODELS_ROOT未设置，确保使用本地路径
        if (server_settings_.models_root.find("/app/") == 0) {
            server_settings_.models_root = "./assets";
        }
    }
}

std::string ServerConfig::get_environment_name() const {
    switch (run_env_) {
        case RunEnvironment::DOCKER:
            return "Docker";
        case RunEnvironment::LOCAL:
            return "Local";
        case RunEnvironment::AUTO:
            return "Auto";
        default:
            return "Unknown";
    }
}

int ServerConfig::get_env_int(const char* name, int default_value) const {
    const char* env_val = std::getenv(name);
    if (env_val != nullptr) {
        try {
            return std::stoi(env_val);
        } catch (const std::exception& e) {
            LOG_WARN("CONFIG", "Invalid integer value for " << name << ": " << env_val 
                     << ", using default: " << default_value);
        }
    }
    return default_value;
}

float ServerConfig::get_env_float(const char* name, float default_value) const {
    const char* env_val = std::getenv(name);
    if (env_val != nullptr) {
        try {
            return std::stof(env_val);
        } catch (const std::exception& e) {
            LOG_WARN("CONFIG", "Invalid float value for " << name << ": " << env_val 
                     << ", using default: " << default_value);
        }
    }
    return default_value;
}

std::string ServerConfig::get_env_string(const char* name, const std::string& default_value) const {
    const char* env_val = std::getenv(name);
    return env_val ? std::string(env_val) : default_value;
}

bool ServerConfig::get_env_bool(const char* name, bool default_value) const {
    const char* env_val = std::getenv(name);
    if (env_val != nullptr) {
        std::string val(env_val);
        // 转换为小写
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        
        if (val == "true" || val == "1" || val == "yes" || val == "on") {
            return true;
        } else if (val == "false" || val == "0" || val == "no" || val == "off") {
            return false;
        } else {
            LOG_WARN("CONFIG", "Invalid boolean value for " << name << ": " << env_val 
                     << ", using default: " << (default_value ? "true" : "false"));
        }
    }
    return default_value;
}

void ServerConfig::load_from_environment() {
    LOG_INFO("CONFIG", "Loading configuration from environment variables");
    LOG_INFO("CONFIG", "Running in " << get_environment_name() << " environment");
    
    // ASR配置
    asr_config_.pool_size = get_env_int("ASR_POOL_SIZE", asr_config_.pool_size);
    asr_config_.num_threads = get_env_int("ASR_NUM_THREADS", asr_config_.num_threads);
    asr_config_.acquire_timeout_ms = get_env_int("ASR_ACQUIRE_TIMEOUT_MS", asr_config_.acquire_timeout_ms);
    asr_config_.model_name = get_env_string("ASR_MODEL_NAME", asr_config_.model_name);
    asr_config_.use_itn = get_env_bool("ASR_USE_ITN", asr_config_.use_itn);
    asr_config_.language = get_env_string("ASR_LANGUAGE", asr_config_.language);
    asr_config_.debug = get_env_bool("ASR_DEBUG", asr_config_.debug);
    
    // VAD配置
    vad_config_.threshold = get_env_float("VAD_THRESHOLD", vad_config_.threshold);
    vad_config_.min_silence_duration = get_env_float("VAD_MIN_SILENCE_DURATION", vad_config_.min_silence_duration);
    vad_config_.min_speech_duration = get_env_float("VAD_MIN_SPEECH_DURATION", vad_config_.min_speech_duration);
    vad_config_.max_speech_duration = get_env_float("VAD_MAX_SPEECH_DURATION", vad_config_.max_speech_duration);
    vad_config_.sample_rate = get_env_float("VAD_SAMPLE_RATE", vad_config_.sample_rate);
    vad_config_.window_size = get_env_int("VAD_WINDOW_SIZE", vad_config_.window_size);
    vad_config_.debug = get_env_bool("VAD_DEBUG", vad_config_.debug);
    
    // VAD池配置
    vad_pool_config_.min_pool_size = static_cast<size_t>(get_env_int("VAD_POOL_MIN_SIZE", static_cast<int>(vad_pool_config_.min_pool_size)));
    vad_pool_config_.max_pool_size = static_cast<size_t>(get_env_int("VAD_POOL_MAX_SIZE", static_cast<int>(vad_pool_config_.max_pool_size)));
    vad_pool_config_.acquire_timeout_ms = get_env_int("VAD_POOL_ACQUIRE_TIMEOUT_MS", vad_pool_config_.acquire_timeout_ms);
    
    // 服务器设置
    server_settings_.port = get_env_int("SERVER_PORT", server_settings_.port);
    server_settings_.models_root = get_env_string("MODELS_ROOT", server_settings_.models_root);
    server_settings_.log_level = get_env_string("LOG_LEVEL", server_settings_.log_level);
    server_settings_.max_connections = get_env_int("MAX_CONNECTIONS", server_settings_.max_connections);
    server_settings_.connection_timeout_s = get_env_int("CONNECTION_TIMEOUT_S", server_settings_.connection_timeout_s);
    
    // 性能配置
    performance_config_.enable_memory_optimization = get_env_bool("ENABLE_MEMORY_OPTIMIZATION", performance_config_.enable_memory_optimization);
    performance_config_.max_audio_buffer_size = static_cast<size_t>(get_env_int("MAX_AUDIO_BUFFER_SIZE", static_cast<int>(performance_config_.max_audio_buffer_size)));
    performance_config_.gc_interval_s = get_env_int("GC_INTERVAL_S", performance_config_.gc_interval_s);
    performance_config_.enable_performance_logging = get_env_bool("ENABLE_PERFORMANCE_LOGGING", performance_config_.enable_performance_logging);
}

void ServerConfig::load_from_args(int argc, char* argv[]) {
    LOG_INFO("CONFIG", "Loading configuration from command line arguments");
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        }
        // 服务器设置
        else if (arg == "--port" && i + 1 < argc) {
            server_settings_.port = std::stoi(argv[++i]);
        }
        else if (arg == "--models-root" && i + 1 < argc) {
            server_settings_.models_root = argv[++i];
        }
        else if (arg == "--log-level" && i + 1 < argc) {
            server_settings_.log_level = argv[++i];
        }
        else if (arg == "--max-connections" && i + 1 < argc) {
            server_settings_.max_connections = std::stoi(argv[++i]);
        }
        // ASR配置
        else if (arg == "--asr-pool-size" && i + 1 < argc) {
            asr_config_.pool_size = std::stoi(argv[++i]);
        }
        else if (arg == "--asr-threads" && i + 1 < argc) {
            asr_config_.num_threads = std::stoi(argv[++i]);
        }
        else if (arg == "--asr-timeout" && i + 1 < argc) {
            asr_config_.acquire_timeout_ms = std::stoi(argv[++i]);
        }
        else if (arg == "--asr-model" && i + 1 < argc) {
            asr_config_.model_name = argv[++i];
        }
        else if (arg == "--asr-language" && i + 1 < argc) {
            asr_config_.language = argv[++i];
        }
        else if (arg == "--asr-use-itn") {
            asr_config_.use_itn = true;
        }
        else if (arg == "--asr-no-itn") {
            asr_config_.use_itn = false;
        }
        else if (arg == "--asr-debug") {
            asr_config_.debug = true;
        }
        // VAD配置
        else if (arg == "--vad-threshold" && i + 1 < argc) {
            vad_config_.threshold = std::stof(argv[++i]);
        }
        else if (arg == "--vad-min-silence" && i + 1 < argc) {
            vad_config_.min_silence_duration = std::stof(argv[++i]);
        }
        else if (arg == "--vad-min-speech" && i + 1 < argc) {
            vad_config_.min_speech_duration = std::stof(argv[++i]);
        }
        else if (arg == "--vad-max-speech" && i + 1 < argc) {
            vad_config_.max_speech_duration = std::stof(argv[++i]);
        }
        else if (arg == "--vad-pool-min" && i + 1 < argc) {
            vad_pool_config_.min_pool_size = static_cast<size_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--vad-pool-max" && i + 1 < argc) {
            vad_pool_config_.max_pool_size = static_cast<size_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--vad-debug") {
            vad_config_.debug = true;
        }
        // 性能配置
        else if (arg == "--enable-memory-opt") {
            performance_config_.enable_memory_optimization = true;
        }
        else if (arg == "--disable-memory-opt") {
            performance_config_.enable_memory_optimization = false;
        }
        else if (arg == "--enable-perf-logging") {
            performance_config_.enable_performance_logging = true;
        }
        else if (arg == "--max-buffer-size" && i + 1 < argc) {
            performance_config_.max_audio_buffer_size = static_cast<size_t>(std::stoi(argv[++i]));
        }
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            exit(1);
        }
    }
}

bool ServerConfig::validate() const {
    bool valid = true;
    
    // 验证ASR配置
    if (asr_config_.pool_size <= 0 || asr_config_.pool_size > 50) {
        LOG_ERROR("CONFIG", "Invalid ASR pool size: " << asr_config_.pool_size << " (must be 1-50)");
        valid = false;
    }
    
    if (asr_config_.num_threads <= 0 || asr_config_.num_threads > 32) {
        LOG_ERROR("CONFIG", "Invalid ASR thread count: " << asr_config_.num_threads << " (must be 1-32)");
        valid = false;
    }
    
    if (asr_config_.acquire_timeout_ms <= 0) {
        LOG_ERROR("CONFIG", "Invalid ASR acquire timeout: " << asr_config_.acquire_timeout_ms);
        valid = false;
    }
    
    // 验证VAD配置
    if (vad_config_.threshold < 0.0f || vad_config_.threshold > 1.0f) {
        LOG_ERROR("CONFIG", "Invalid VAD threshold: " << vad_config_.threshold << " (must be 0.0-1.0)");
        valid = false;
    }
    
    if (vad_config_.min_silence_duration <= 0.0f) {
        LOG_ERROR("CONFIG", "Invalid VAD min silence duration: " << vad_config_.min_silence_duration);
        valid = false;
    }
    
    if (vad_config_.min_speech_duration <= 0.0f) {
        LOG_ERROR("CONFIG", "Invalid VAD min speech duration: " << vad_config_.min_speech_duration);
        valid = false;
    }
    
    if (vad_config_.max_speech_duration <= vad_config_.min_speech_duration) {
        LOG_ERROR("CONFIG", "VAD max speech duration must be greater than min speech duration");
        valid = false;
    }
    
    // 验证VAD池配置
    if (vad_pool_config_.min_pool_size == 0 || vad_pool_config_.min_pool_size > vad_pool_config_.max_pool_size) {
        LOG_ERROR("CONFIG", "Invalid VAD pool sizes: min=" << vad_pool_config_.min_pool_size 
                  << ", max=" << vad_pool_config_.max_pool_size);
        valid = false;
    }
    
    // 验证服务器设置
    if (server_settings_.port <= 0 || server_settings_.port > 65535) {
        LOG_ERROR("CONFIG", "Invalid server port: " << server_settings_.port);
        valid = false;
    }
    
    if (server_settings_.max_connections <= 0) {
        LOG_ERROR("CONFIG", "Invalid max connections: " << server_settings_.max_connections);
        valid = false;
    }
    
    return valid;
}

void ServerConfig::print_config() const {
    LOG_INFO("CONFIG", "=== Current Configuration ===");
    
    // 环境信息
    LOG_INFO("CONFIG", "[Environment Information]");
    LOG_INFO("CONFIG", "  Runtime Environment: " << get_environment_name());
    LOG_INFO("CONFIG", "  Config Adapted: Yes");
    
    // 服务器设置
    LOG_INFO("CONFIG", "[Server Settings]");
    LOG_INFO("CONFIG", "  Port: " << server_settings_.port);
    LOG_INFO("CONFIG", "  Models Root: " << server_settings_.models_root);
    LOG_INFO("CONFIG", "  Log Level: " << server_settings_.log_level);
    LOG_INFO("CONFIG", "  Max Connections: " << server_settings_.max_connections);
    LOG_INFO("CONFIG", "  Connection Timeout: " << server_settings_.connection_timeout_s << "s");
    
    // ASR配置
    LOG_INFO("CONFIG", "[ASR Configuration]");
    LOG_INFO("CONFIG", "  Pool Size: " << asr_config_.pool_size);
    LOG_INFO("CONFIG", "  Threads: " << asr_config_.num_threads);
    LOG_INFO("CONFIG", "  Acquire Timeout: " << asr_config_.acquire_timeout_ms << "ms");
    LOG_INFO("CONFIG", "  Model Name: " << asr_config_.model_name);
    LOG_INFO("CONFIG", "  Language: " << asr_config_.language);
    LOG_INFO("CONFIG", "  Use ITN: " << (asr_config_.use_itn ? "true" : "false"));
    LOG_INFO("CONFIG", "  Debug: " << (asr_config_.debug ? "true" : "false"));
    
    // VAD配置
    LOG_INFO("CONFIG", "[VAD Configuration]");
    LOG_INFO("CONFIG", "  Threshold: " << vad_config_.threshold);
    LOG_INFO("CONFIG", "  Min Silence Duration: " << vad_config_.min_silence_duration << "s");
    LOG_INFO("CONFIG", "  Min Speech Duration: " << vad_config_.min_speech_duration << "s");
    LOG_INFO("CONFIG", "  Max Speech Duration: " << vad_config_.max_speech_duration << "s");
    LOG_INFO("CONFIG", "  Sample Rate: " << vad_config_.sample_rate << "Hz");
    LOG_INFO("CONFIG", "  Window Size: " << vad_config_.window_size);
    LOG_INFO("CONFIG", "  Debug: " << (vad_config_.debug ? "true" : "false"));
    
    // VAD池配置
    LOG_INFO("CONFIG", "[VAD Pool Configuration]");
    LOG_INFO("CONFIG", "  Min Pool Size: " << vad_pool_config_.min_pool_size);
    LOG_INFO("CONFIG", "  Max Pool Size: " << vad_pool_config_.max_pool_size);
    LOG_INFO("CONFIG", "  Acquire Timeout: " << vad_pool_config_.acquire_timeout_ms << "ms");
    
    // 性能配置
    LOG_INFO("CONFIG", "[Performance Configuration]");
    LOG_INFO("CONFIG", "  Memory Optimization: " << (performance_config_.enable_memory_optimization ? "enabled" : "disabled"));
    LOG_INFO("CONFIG", "  Max Audio Buffer Size: " << performance_config_.max_audio_buffer_size << " bytes");
    LOG_INFO("CONFIG", "  GC Interval: " << performance_config_.gc_interval_s << "s");
    LOG_INFO("CONFIG", "  Performance Logging: " << (performance_config_.enable_performance_logging ? "enabled" : "disabled"));
    
    LOG_INFO("CONFIG", "=== End Configuration ===");
}

void ServerConfig::print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Server Options:" << std::endl;
    std::cout << "  --port PORT                    Server port (default: 8000)" << std::endl;
    std::cout << "  --models-root PATH             Path to models directory (default: ./assets)" << std::endl;
    std::cout << "  --log-level LEVEL              Log level: DEBUG, INFO, WARN, ERROR (default: INFO)" << std::endl;
    std::cout << "  --max-connections NUM          Maximum concurrent connections (default: 100)" << std::endl;
    std::cout << std::endl;
    std::cout << "ASR Options:" << std::endl;
    std::cout << "  --asr-pool-size NUM            ASR model pool size (default: 2)" << std::endl;
    std::cout << "  --asr-threads NUM              ASR threads per model (default: 2)" << std::endl;
    std::cout << "  --asr-timeout MS               ASR acquire timeout in ms (default: 5000)" << std::endl;
    std::cout << "  --asr-model NAME               ASR model name (default: sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17)" << std::endl;
    std::cout << "  --asr-language LANG            ASR language (default: auto)" << std::endl;
    std::cout << "  --asr-use-itn/--asr-no-itn     Enable/disable ITN (default: enabled)" << std::endl;
    std::cout << "  --asr-debug                    Enable ASR debug mode" << std::endl;
    std::cout << std::endl;
    std::cout << "VAD Options:" << std::endl;
    std::cout << "  --vad-threshold FLOAT          VAD threshold 0.0-1.0 (default: 0.5)" << std::endl;
    std::cout << "  --vad-min-silence FLOAT        Min silence duration in seconds (default: 0.25)" << std::endl;
    std::cout << "  --vad-min-speech FLOAT         Min speech duration in seconds (default: 0.25)" << std::endl;
    std::cout << "  --vad-max-speech FLOAT         Max speech duration in seconds (default: 8.0)" << std::endl;
    std::cout << "  --vad-pool-min NUM             VAD pool min size (default: 2)" << std::endl;
    std::cout << "  --vad-pool-max NUM             VAD pool max size (default: 10)" << std::endl;
    std::cout << "  --vad-debug                    Enable VAD debug mode" << std::endl;
    std::cout << std::endl;
    std::cout << "Performance Options:" << std::endl;
    std::cout << "  --enable-memory-opt            Enable memory optimization" << std::endl;
    std::cout << "  --disable-memory-opt           Disable memory optimization" << std::endl;
    std::cout << "  --enable-perf-logging          Enable performance logging" << std::endl;
    std::cout << "  --max-buffer-size BYTES        Max audio buffer size (default: 1048576)" << std::endl;
    std::cout << std::endl;
    std::cout << "Environment Variables:" << std::endl;
    std::cout << "  SERVER_PORT, MODELS_ROOT, LOG_LEVEL, MAX_CONNECTIONS" << std::endl;
    std::cout << "  ASR_POOL_SIZE, ASR_NUM_THREADS, ASR_ACQUIRE_TIMEOUT_MS, ASR_MODEL_NAME" << std::endl;
    std::cout << "  ASR_LANGUAGE, ASR_USE_ITN, ASR_DEBUG" << std::endl;
    std::cout << "  VAD_THRESHOLD, VAD_MIN_SILENCE_DURATION, VAD_MIN_SPEECH_DURATION" << std::endl;
    std::cout << "  VAD_MAX_SPEECH_DURATION, VAD_POOL_MIN_SIZE, VAD_POOL_MAX_SIZE, VAD_DEBUG" << std::endl;
    std::cout << "  ENABLE_MEMORY_OPTIMIZATION, MAX_AUDIO_BUFFER_SIZE, ENABLE_PERFORMANCE_LOGGING" << std::endl;
    std::cout << std::endl;
    std::cout << "  --help, -h                     Show this help message" << std::endl;
}
