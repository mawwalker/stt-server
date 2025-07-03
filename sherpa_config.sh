#!/bin/bash

# sherpa-onnx 统一配置管理脚本
# 此脚本定义了所有安装脚本共用的CMake配置选项
# 如果需要调整sherpa-onnx的编译选项，只需修改此文件

# sherpa-onnx 优化构建配置
# 基于WebSocket ASR项目的实际需求，只启用必要组件

generate_cmake_options() {
    local install_prefix="$1"
    local use_shared_libs="$2"
    
    local cmake_options=""
    
    # 基础选项
    cmake_options="${cmake_options} -DCMAKE_BUILD_TYPE=Release"
    cmake_options="${cmake_options} -DCMAKE_INSTALL_PREFIX=${install_prefix}"
    
    # 共享库选项（仅在需要时启用）
    if [[ "$use_shared_libs" == "ON" ]]; then
        cmake_options="${cmake_options} -DBUILD_SHARED_LIBS=ON"
    fi
    
    # 启用的组件 ✅
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_C_API=ON"
    
    # 禁用的组件 ❌ (大幅减少编译时间和依赖)
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_PYTHON=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_TESTS=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_CHECK=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_JNI=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_GPU=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_DIRECTML=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_WASM=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_BINARY=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_TTS=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_BUILD_C_API_EXAMPLES=OFF"
    cmake_options="${cmake_options} -DSHERPA_ONNX_ENABLE_RKNN=OFF"
    
    echo "$cmake_options"
}

# 显示配置信息
show_config_info() {
    echo "sherpa-onnx 优化构建配置:"
    echo "✅ 启用组件: ASR (OfflineRecognizer) + VAD (VoiceActivityDetector) + C++ API"
    echo "❌ 禁用组件: TTS, Speaker Diarization, Python, WebSocket, GPU, examples, etc."
    echo "📈 预期收益: ~60-70% 更快编译, ~50% 更少磁盘空间"
}

# 获取GCC版本并确定是否使用共享库
get_shared_libs_option() {
    if command -v gcc &> /dev/null; then
        local gcc_version=$(gcc -dumpversion | cut -d. -f1)
        if [[ $gcc_version -le 10 ]]; then
            echo "ON"
        else
            echo "OFF"
        fi
    else
        echo "OFF"
    fi
}

# 如果此脚本被直接执行，显示配置信息
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "sherpa-onnx 配置管理脚本"
    echo "========================="
    show_config_info
    echo ""
    echo "使用示例:"
    echo "  source sherpa_config.sh"
    echo "  cmake_opts=\$(generate_cmake_options \"/usr/local\" \"OFF\")"
    echo "  echo \$cmake_opts"
fi
