# Multi-stage build Dockerfile for WebSocket ASR Server
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
    # Python (sherpa-onnx 构建可能需要)
    python3 \
    python3-pip \
    python3-dev \
    # 清理缓存
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /workspace

# 安装 sherpa-onnx (按照官方教程)
RUN echo "Installing sherpa-onnx..." && \
    git clone https://github.com/k2-fsa/sherpa-onnx.git && \
    cd sherpa-onnx && \
    mkdir build && \
    cd build && \
    # 使用 Release 模式构建，默认为静态库
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          .. && \
    # 使用所有可用核心进行编译
    make -j$(nproc) && \
    make install && \
    # 更新动态链接库缓存
    ldconfig && \
    # 清理构建文件以减小镜像大小
    cd /workspace && \
    rm -rf sherpa-onnx

# 复制项目源代码
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
    # 清理缓存
    && rm -rf /var/lib/apt/lists/*

# 从构建阶段复制已安装的 sherpa-onnx 库
COPY --from=builder /usr/local/lib/libsherpa-onnx* /usr/local/lib/
COPY --from=builder /usr/local/lib/libonnxruntime* /usr/local/lib/
COPY --from=builder /usr/local/lib/libkaldi* /usr/local/lib/
COPY --from=builder /usr/local/lib/libssentencepiece* /usr/local/lib/
COPY --from=builder /usr/local/lib/libespeak* /usr/local/lib/
COPY --from=builder /usr/local/lib/libpiper* /usr/local/lib/
COPY --from=builder /usr/local/lib/libkissfft* /usr/local/lib/
COPY --from=builder /usr/local/lib/libucd* /usr/local/lib/
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

# 设置默认环境变量
ENV MODELS_ROOT=/app/assets
ENV PORT=8000
ENV THREADS=2
ENV LOG_LEVEL=INFO

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=3 \
    CMD curl -f http://localhost:${PORT}/health || exit 1

# 启动命令
CMD ["sh", "-c", "websocket_asr_server --models-root ${MODELS_ROOT} --port ${PORT} --threads ${THREADS} --log-level ${LOG_LEVEL}"]
