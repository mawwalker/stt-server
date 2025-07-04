# Multi-stage build Dockerfile for WebSocket ASR Server
# Using optimized sherpa-onnx build for faster compilation
FROM ubuntu:22.04 AS builder

# 设置非交互式安装，避免构建时卡住
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 安装基础构建工具和依赖
RUN apt-get update && apt-get install -y \
    # 基础构建工具
    build-essential \
    cmake \
    git \
    pkg-config \
    wget \
    curl \
    # C++ 库依赖
    libjsoncpp-dev \
    libwebsocketpp-dev \
    libasio-dev \
    # Boost 作为 ASIO 的备选方案
    libboost-system-dev \
    libboost-filesystem-dev \
    # 其他可能需要的依赖
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    liblzma-dev \
    libffi-dev \
    libsqlite3-dev \
    # 清理缓存
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /workspace

COPY install_sherpa_onnx_docker.sh /workspace/
COPY sherpa_config.sh /workspace/

# 安装 sherpa-onnx 使用优化脚本 (大幅提升编译效率)
RUN echo "Installing sherpa-onnx with optimized build..." && \
    chmod +x install_sherpa_onnx_docker.sh && \
    ./install_sherpa_onnx_docker.sh && \
    # 更新动态链接库缓存
    ldconfig

# 复制项目源代码和安装脚本
COPY . /workspace/websocket_asr_server/

# 设置项目工作目录
WORKDIR /workspace/websocket_asr_server

# 构建项目
RUN echo "Building WebSocket ASR Server..." && \
    chmod +x build.sh && \
    ./build.sh

# 运行时镜像 (减小最终镜像大小)
FROM ubuntu:22.04 AS runtime

# 设置时区和语言环境
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    # C++ 运行时库
    libstdc++6 \
    libgcc-s1 \
    # JSON 库
    libjsoncpp25 \
    # Boost 系统库 (如果使用 Boost ASIO)
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    # SSL 支持
    libssl3 \
    # 其他运行时库
    zlib1g \
    libbz2-1.0 \
    liblzma5 \
    libffi8 \
    libsqlite3-0 \
    # 网络工具 (用于健康检查)
    net-tools \
    # 清理缓存
    && rm -rf /var/lib/apt/lists/*

# 从构建阶段复制已安装的 sherpa-onnx 库 (仅复制必需的库)
COPY --from=builder /usr/local/lib/libsherpa-onnx-core* /usr/local/lib/
COPY --from=builder /usr/local/lib/libsherpa-onnx-cxx-api* /usr/local/lib/
COPY --from=builder /usr/local/lib/libsherpa-onnx-c-api* /usr/local/lib/
COPY --from=builder /usr/local/lib/libsherpa-onnx-fst* /usr/local/lib/
COPY --from=builder /usr/local/lib/libsherpa-onnx-kaldifst* /usr/local/lib/
COPY --from=builder /usr/local/lib/libonnxruntime* /usr/local/lib/
COPY --from=builder /usr/local/lib/libkaldi* /usr/local/lib/
COPY --from=builder /usr/local/lib/libssentencepiece* /usr/local/lib/
COPY --from=builder /usr/local/lib/libkissfft* /usr/local/lib/
COPY --from=builder /usr/local/lib/libucd* /usr/local/lib/
# 不复制 TTS 相关库 (espeak, piper) 因为我们禁用了它们
COPY --from=builder /usr/local/include/sherpa-onnx/ /usr/local/include/sherpa-onnx/

# 从构建阶段复制编译好的可执行文件
COPY --from=builder /workspace/websocket_asr_server/build/websocket_asr_server /usr/local/bin/

# 复制模型文件
COPY --from=builder /workspace/websocket_asr_server/assets/ /app/assets/

# 复制示例文件
COPY --from=builder /workspace/websocket_asr_server/examples/ /app/examples/

# 更新动态链接库缓存
RUN ldconfig

# 设置工作目录
WORKDIR /app

# 创建非 root 用户
RUN groupadd -r asruser && useradd -r -g asruser asruser && \
    chown -R asruser:asruser /app

# 切换到非 root 用户
USER asruser

# 暴露端口
EXPOSE 8000

# 设置默认环境变量 (可被docker-compose或环境变量覆盖)
ENV MODELS_ROOT=/app/assets
ENV SERVER_PORT=8000
ENV LOG_LEVEL=INFO
ENV MAX_CONNECTIONS=100
ENV CONNECTION_TIMEOUT_S=300

# ASR配置默认值
ENV ASR_NUM_THREADS=4
ENV ASR_MODEL_NAME=sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17
ENV ASR_LANGUAGE=auto
ENV ASR_USE_ITN=true
ENV ASR_DEBUG=false

# VAD配置默认值
ENV VAD_THRESHOLD=0.5
ENV VAD_MIN_SILENCE_DURATION=0.25
ENV VAD_MIN_SPEECH_DURATION=0.25
ENV VAD_MAX_SPEECH_DURATION=8.0
ENV VAD_SAMPLE_RATE=16000
ENV VAD_WINDOW_SIZE=100
ENV VAD_DEBUG=false

# VAD池配置默认值
ENV VAD_POOL_MIN_SIZE=2
ENV VAD_POOL_MAX_SIZE=10

# 性能优化配置默认值
ENV ENABLE_MEMORY_OPTIMIZATION=true
ENV MAX_AUDIO_BUFFER_SIZE=1048576
ENV GC_INTERVAL_S=60
ENV ENABLE_PERFORMANCE_LOGGING=false

# 健康检查 (使用简单的端口检查，因为程序可能没有 /health 端点)
HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=3 \
    CMD netstat -tuln | grep :${SERVER_PORT} || exit 1

# 直接使用二进制文件作为入口点，参考 run.sh 的本地启动逻辑
ENTRYPOINT ["/usr/local/bin/websocket_asr_server"]
CMD []
