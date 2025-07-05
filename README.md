# C++ WebSocket ASR Server

基于 sherpa-onnx 和 SenseVoice 的 C++ WebSocket 流式语音识别服务器。

## 📋 目录导航

- [功能特性](#功能特性)
- [统一配置系统](#统一配置系统)
- [快速开始](#快速开始)
- [安装依赖](#安装依赖)
- [模型准备](#模型准备)
- [编译运行](#编译运行)
- [Docker部署](#docker部署)
- [WebSocket接口](#websocket接口)
- [性能优化](#性能优化)
- [故障排除](#故障排除)
- [架构说明](#架构说明)
- [开发指南](#开发指南)

## 🚀 功能特性

- **实时流式语音识别** - 基于 SenseVoice 模型的多语言识别
- **WebSocket 接口** - 兼容现有的 Python 客户端
- **多语言支持** - 中文、英文、日文、韩文、粤语
- **内置 VAD** - Silero VAD 语音活动检测
- **高并发** - 支持多客户端同时连接
- **低延迟** - C++ 实现，性能优异
- **容器化部署** - 完整的 Docker 解决方案
- **🆕 统一配置** - 一个.env文件同时支持本地和Docker环境
- **🆕 环境自适应** - 自动检测运行环境并适配配置

## 🔧 统一配置系统

### 一键启动

```bash
# 方式1: 使用统一启动脚本
./run.sh local          # 本地启动
./run.sh docker         # Docker启动
./run.sh build          # 编译项目
./run.sh status         # 查看状态

# 方式2: 使用 Makefile
make run                # 本地运行（默认）
make run-docker         # Docker运行
make build              # 编译
make status             # 查看状态
```

### 配置文件统一

只需要一个 `.env` 配置文件，支持本地和 Docker 环境：

```bash
# .env 文件 - 统一配置
SERVER_PORT=8000
LOG_LEVEL=INFO
ASR_POOL_SIZE=2
ASR_NUM_THREADS=2
ASR_MODEL_NAME=sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17
VAD_THRESHOLD=0.5
ENABLE_MEMORY_OPTIMIZATION=true
# ... 完整配置见 .env.example
```

### 环境自动检测

系统会自动检测运行环境：
- **本地环境**: 使用 `./assets` 作为模型目录
- **Docker环境**: 自动使用 `/app/assets` 作为模型目录
- **自动适配**: 无需手动修改配置文件
- **Docker环境**: 自动使用 `/app/assets` 作为模型目录
- **手动指定**: 设置 `RUN_ENVIRONMENT=local|docker`

### 配置优先级

**命令行参数** > **环境变量** > **默认值**

```bash
# 环境变量方式
export ASR_POOL_SIZE=8
./start.sh local

# 命令行参数方式 (会覆盖环境变量)
./build/websocket_asr_server --asr-pool-size 8
```

### 主要配置参数

| 类型 | 参数 | 环境变量 | 默认值 | 说明 |
|------|------|----------|--------|------|
| 服务器 | `--port` | `SERVER_PORT` | 8000 | 服务端口 |
| 服务器 | `--models-root` | `MODELS_ROOT` | ./assets | 模型目录 |
| ASR | `--asr-pool-size` | `ASR_POOL_SIZE` | 2 | ASR模型池大小 |
| ASR | `--asr-threads` | `ASR_NUM_THREADS` | 2 | ASR线程数 |
| VAD | `--vad-pool-max` | `VAD_POOL_MAX_SIZE` | 10 | VAD池最大大小 |
| VAD | `--vad-threshold` | `VAD_THRESHOLD` | 0.5 | VAD检测阈值 |

📖 **完整配置文档**: [CONFIG.md](CONFIG.md)

## ⚡ 快速开始

### 1. 准备项目
```bash
# 克隆项目并进入目录
git clone <your-repository-url>
cd stt

# 确保配置文件存在（已包含默认配置）
ls .env  # 如果不存在，复制: cp .env.example .env
```

### 2. 一键启动

**方式1: 统一启动脚本（推荐）**
```bash
# 给脚本执行权限
chmod +x run.sh

# 本地启动（自动编译）
./run.sh local

# Docker启动
./run.sh docker

# 查看帮助
./run.sh help
```

**方式2: 使用 Makefile**
```bash
# 本地运行
make run                # 等价于 ./run.sh local

# Docker运行  
make run-docker         # 等价于 ./run.sh docker

# 查看所有命令
make help
```

### 3. 验证服务
```bash
# 查看服务状态
./run.sh status
# 或
make status

# 服务应该在 http://localhost:8000 启动
# WebSocket 端点: ws://localhost:8000/ws
```

### 4. 常用操作
```bash
./run.sh build          # 编译项目
./run.sh logs           # 查看Docker日志
./run.sh stop           # 停止Docker服务
./run.sh clean          # 清理构建文件

# 本地服务使用 Ctrl+C 停止
```

```bash
# 本地启动
mkdir -p build && cd build
cmake .. && make -j$(nproc)
cd ..
source .env  # 加载配置
./build/websocket_asr_server

# Docker启动
docker-compose up --build -d
```

### 方法三：自定义配置

```bash
# 编辑.env文件
vim .env

# 或者临时设置环境变量
export ASR_POOL_SIZE=8
export SERVER_PORT=8080
export LOG_LEVEL=DEBUG

# 启动
./start.sh local
```

```bash
# 完全自定义配置
./build/websocket_asr_server \
  --port 8080 \
  --asr-pool-size 4 \
  --asr-threads 8 \
  --vad-pool-max 20 \
  --log-level INFO \
  --max-connections 100
```

### 方法二：Docker 部署（推荐生产环境）

```bash
# 1. 克隆项目
git clone <repository-url>
cd websocket-asr-server

# 2. build

docker built -t stt-server .
```

### 方法三：分步安装

```bash

# 1. 安装 sherpa-onnx
./install_sherpa_onnx.sh

# 3. 编译项目
./build.sh

# 4. 运行服务
./build/websocket_asr_server
```

## 📦 安装依赖

### 系统依赖
```bash
# Ubuntu/Debian
sudo apt-get install -y \
    cmake \
    build-essential \
    libjsoncpp-dev \
    libwebsocketpp-dev \
    libasio-dev \
    pkg-config

# 或者使用 Boost ASIO (如果没有独立的 ASIO)
sudo apt-get install -y libboost-system-dev
```

### sherpa-onnx 安装（优化版本）

本项目提供了**优化的 sherpa-onnx 安装方案**，只编译项目实际需要的组件：

- ✅ **启用**: ASR识别 + VAD检测 + C++ API
- ❌ **禁用**: TTS、说话人分离、Python绑定、WebSocket、GPU等
- 🚀 **效果**: 减少60-70%编译时间，减少50%安装空间

#### 方法一：自动安装脚本（推荐）

```bash
# 用户目录安装（推荐）
./install_sherpa_onnx.sh

# 系统目录安装
./install_sherpa_onnx.sh --system

# 查看安装选项
./install_sherpa_onnx.sh --help

# 如果使用 GCC<=10，使用共享库
./install_sherpa_onnx.sh --shared

# 自定义并行编译数量
./install_sherpa_onnx.sh -j 8
```

#### 方法二：简化版安装
```bash
./install_sherpa_onnx_simple.sh
```

#### 方法三：手动安装
```bash
# 1. 克隆 sherpa-onnx 源码
git clone https://github.com/k2-fsa/sherpa-onnx
cd sherpa-onnx
mkdir build && cd build

# 2. 使用优化配置
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
      -DSHERPA_ONNX_ENABLE_PYTHON=OFF \
      -DSHERPA_ONNX_ENABLE_TESTS=OFF \
      -DSHERPA_ONNX_ENABLE_TTS=OFF \
      -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
      -DSHERPA_ONNX_ENABLE_C_API=ON \
      ..

# 3. 编译安装
make -j6
make install
```

#### 环境设置

安装完成后设置环境变量：

```bash
# 方法一：使用脚本
source ./setup_env.sh

# 方法二：手动设置
export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH="$HOME/.local/bin:$PATH"
```

## 🎯 模型准备

确保在 `assets` 目录下有以下模型文件：

```
assets/
├── sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/
│   ├── model.onnx
│   └── tokens.txt
└── silero_vad/
    └── silero_vad.onnx
```

### 模型下载

```bash
# SenseVoice 模型 - 支持中英日韩粤语识别
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
tar xvf sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
mv sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17 assets/

# VAD 模型 - 语音活动检测
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx
mkdir -p assets/silero_vad
mv silero_vad.onnx assets/silero_vad/
```

> **模型说明**: SenseVoice 模型来自 FunAudioLLM/SenseVoice 项目，已转换为 ONNX 格式用于 sherpa-onnx。

## 🔨 编译运行

### 编译项目

```bash
# 给构建脚本执行权限
chmod +x build.sh

# 编译项目（会自动检测 sherpa-onnx 安装位置）
./build.sh
```

### 启动服务器

```bash
# 基本启动
./build/websocket_asr_server

# 自定义参数
./build/websocket_asr_server \
    --models-root ./assets \
    --port 8000 \
    --threads 4

# 查看所有参数
./build/websocket_asr_server --help
```

### 服务器参数

- `--port PORT`: 服务器端口（默认：8000）
- `--models-root PATH`: 模型文件目录（默认：./assets）
- `--threads NUM`: 推理线程数（默认：2）
- `--help`: 显示帮助信息

## 🐳 Docker部署

### 快速部署

```bash
# 方法一：使用构建脚本
./docker_build.sh
docker run -d --name asr-server -p 8000:8000 websocket-asr-server:latest

# 方法二：直接构建
docker build -t websocket-asr-server .
docker run -p 8000:8000 websocket-asr-server:latest

# 方法三：使用 Docker Compose（推荐）
docker compose up -d
```

### Docker 健康检查

```bash
# 检查容器状态
docker ps --format "table {{.Names}}\t{{.Status}}"

# 手动健康检查
curl http://localhost:8000/health

# 查看日志
docker logs -f websocket-asr-server
```

## 🌐 WebSocket接口

### 连接方式

**地址**: `ws://localhost:8000/asr?samplerate=16000`

**参数**:
- `samplerate`: 音频采样率（支持 8000, 16000, 22050, 44100 等）

### 通信协议

**发送数据**: PCM 音频数据（16位，单声道，指定采样率）

**接收数据**: JSON 格式的识别结果

```json
{
    "text": "Hello world",
    "finished": false,     // true=最终结果，false=部分结果
    "idx": 0,              // 语音段索引
    "lang": "zh",           // 语言
    "timestamp": [0.1, 0.2], // 时间戳（可选）
    "tokens": ["Hello", "world"]
}
```

### 客户端示例

#### Python WebSocket 客户端

```python
import asyncio
import websockets
import wave
import json

async def test_asr():
    uri = "ws://localhost:8000/asr?samplerate=16000"
    
    async with websockets.connect(uri) as websocket:
        # 发送音频文件
        with wave.open("test.wav", "rb") as wav_file:
            data = wav_file.readframes(1024)
            while data:
                await websocket.send(data)
                data = wav_file.readframes(1024)
        
        # 接收结果
        async for message in websocket:
            result = json.loads(message)
            print(f"识别结果: {result['text']}")
            if result['finished']:
                break

# 运行测试
asyncio.run(test_asr())
```

#### JavaScript 客户端

```javascript
const socket = new WebSocket('ws://localhost:8000/asr?samplerate=16000');

socket.onopen = function(event) {
    console.log('连接已建立');
    // 发送音频数据...
};

socket.onmessage = function(event) {
    const result = JSON.parse(event.data);
    console.log('识别结果:', result.text);
    
    if (result.finished) {
        console.log('识别完成');
    }
};
```


## ⚡ 性能优化

### 编译优化

```bash
# 启用高级优化
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"
make -j$(nproc)
```

### 运行时优化

```bash
# 增加处理线程数
./build/websocket_asr_server --threads 8

# 使用快速存储设备存放模型
# 建议将 assets 目录放在 SSD 上
```

### 系统优化

```bash
# 调整系统参数
echo 'net.core.somaxconn = 1024' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_max_syn_backlog = 1024' >> /etc/sysctl.conf
sysctl -p

# 增加文件描述符限制
ulimit -n 65536
```

## 🔧 故障排除

### 编译问题

#### 问题1：找不到 sherpa-onnx

```bash
./install_sherpa_onnx.sh
```

#### 问题2：缺少依赖库

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y libwebsocketpp-dev libjsoncpp-dev libasio-dev

# CentOS/RHEL
没试过
```

### 运行时问题

#### 问题1：模型文件错误

```bash
# 检查模型文件
ls -la assets/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/
ls -la assets/silero_vad/

# 如果不存在，参考sherpa-onnx，下载需要的两个模型
```

#### 问题2：端口被占用

```bash
# 检查端口占用
netstat -tlnp | grep :8000
lsof -i :8000

# 使用不同端口
./build/websocket_asr_server --port 8001
```

#### 问题3：连接被拒绝

```bash
# 检查防火墙
sudo ufw status
sudo iptables -L

# 检查服务状态
curl http://localhost:8000/health
```

### Docker 问题

#### 问题1：构建失败

```bash
# 清理 Docker 缓存
docker system prune -a

# 无缓存重新构建
docker build --no-cache -t websocket-asr-server .
```

#### 问题2：容器启动失败

```bash
# 查看详细日志
docker logs --details websocket-asr-server

# 进入容器调试
docker exec -it websocket-asr-server /bin/bash
```

### 常见错误及解决

| 错误信息 | 可能原因 | 解决方案 |
|----------|----------|----------|
| `libsherpa-onnx.so: not found` | 库路径未设置 | 运行 `source setup_env.sh` |
| `Model file not found` | 模型路径错误 | 检查 `--models-root` 参数 |
| `Address already in use` | 端口被占用 | 更换端口或终止占用进程 |
| `Connection refused` | 防火墙阻止 | 检查防火墙设置 |

## 🏗️ 架构说明

### 系统架构

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   WebSocket     │    │  WebSocket ASR   │    │   sherpa-onnx   │
│   Client        │◄──►│     Server       │◄──►│   Engine        │
│                 │    │                  │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │  Connection      │    │  Model Files    │
                       │  Manager         │    │  • SenseVoice   │
                       │                  │    │  • Silero VAD   │
                       └──────────────────┘    └─────────────────┘
```

### 核心组件

#### 1. ASREngine
- **功能**: 管理 sherpa-onnx 推理引擎和 VAD
- **文件**: `src/asr_engine.cpp`, `include/asr_engine.h`
- **职责**: 模型加载、推理配置、资源管理

#### 2. ASRSession  
- **功能**: 处理单个 WebSocket 连接的音频流
- **文件**: `src/asr_session.cpp`, `include/asr_session.h`  
- **职责**: 音频处理、VAD检测、结果生成

#### 3. WebSocketASRServer
- **功能**: WebSocket 服务器，管理所有连接
- **文件**: `src/websocket_server.cpp`, `include/websocket_server.h`
- **职责**: 连接管理、消息路由、并发控制

#### 4. Logger
- **功能**: 统一日志系统
- **文件**: `src/logger.cpp`, `include/logger.h`
- **职责**: 日志输出、级别控制、格式化

### 处理流程

```
1. 客户端连接 WebSocket
           ↓
2. 创建 ASRSession 实例
           ↓  
3. 接收 PCM 音频数据
           ↓
4. VAD 检测语音活动
           ↓
5. 流式推理生成部分结果
           ↓
6. VAD 检测语音结束
           ↓
7. 生成最终结果
           ↓
8. 通过 WebSocket 发送 JSON 结果
```

## 👨‍💻 开发指南

### 项目结构

```
├── CMakeLists.txt              # CMake 配置（已优化）
├── build.sh                    # 构建脚本
├── install_sherpa_onnx.sh      # sherpa-onnx 安装脚本（优化版）
├── install_sherpa_onnx_simple.sh # 简化安装脚本
├── setup_env.sh                # 环境设置脚本
├── sherpa_config.sh           # 统一配置管理
├── docker_build.sh            # Docker 构建脚本
├── Dockerfile                 # Docker 配置
├── docker-compose.yml         # Docker Compose 配置
├── main.cpp                   # 程序入口
├── include/                   # 头文件目录
│   ├── asr_engine.h          # ASR 引擎接口
│   ├── asr_session.h         # ASR 会话管理
│   ├── websocket_server.h    # WebSocket 服务器
│   ├── logger.h              # 日志系统
│   └── common.h              # 公共定义
├── src/                      # 源文件目录
│   ├── asr_engine.cpp        # ASR 引擎实现
│   ├── asr_session.cpp       # ASR 会话实现
│   ├── websocket_server.cpp  # WebSocket 服务器实现
│   └── logger.cpp            # 日志实现
├── assets/                   # 模型文件目录
│   ├── sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/
│   └── silero_vad/
```

### 开发环境设置

# 或者手动设置
./install_sherpa_onnx.sh
source ./setup_env.sh

# 3. 编译调试版本
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### 代码规范

- **C++ 标准**: C++17
- **代码风格**: Google C++ Style Guide
- **命名规范**: 
  - 类名: `PascalCase` (如 `ASREngine`)
  - 函数名: `camelCase` (如 `processAudio`)  
  - 变量名: `snake_case` (如 `sample_rate`)
  - 常量: `UPPER_CASE` (如 `MAX_BUFFER_SIZE`)
- **注释**: 使用 Doxygen 格式

### 添加新功能

#### 1. 添加新模型支持

```cpp
// 在 ASREngine::initialize() 中
if (model_type == "new-model") {
    auto config = sherpa_onnx::OfflineRecognizerConfig{};
    config.model_config.model = model_path;
    // ... 配置新模型参数
}
```

#### 2. 自定义 VAD 参数

```cpp
// 在 ASREngine::initialize() 中
vad_config.silero_vad.threshold = 0.5f;        // 检测阈值
vad_config.silero_vad.min_silence_duration = 0.25f;  // 最小静音时长
vad_config.silero_vad.min_speech_duration = 0.25f;   // 最小语音时长
```

#### 3. 添加认证机制

```cpp
// 在 WebSocket 连接处理中
bool authenticate(const std::string& token) {
    // 实现认证逻辑
    return validate_token(token);
}
```

### 测试

#### 内存泄漏检测

```bash
# 使用 Valgrind
valgrind --leak-check=full ./build/websocket_asr_server

# 使用 AddressSanitizer
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address"
```

### 调试技巧

#### 1. 日志调试

```cpp
// 设置日志级别
Logger::getInstance().setLevel(LogLevel::DEBUG);

// 添加调试信息
LOG_DEBUG("Processing audio chunk: {} bytes", chunk_size);
LOG_INFO("VAD detected speech: {:.2f}s", speech_duration);
```

#### 2. GDB 调试

```bash
# 编译调试版本
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# 启动 GDB
gdb ./build/websocket_asr_server
(gdb) run --port 8000
(gdb) bt  # 查看堆栈
```

#### 3. 性能分析

```bash
# 使用 perf
perf record ./build/websocket_asr_server
perf report

# 使用 gperftools
cmake .. -DENABLE_PROFILING=ON
make
```

### 贡献指南

#### 提交信息规范

```
type(scope): description

[optional body]

[optional footer]
```

类型:
- `feat`: 新功能
- `fix`: 修复bug  
- `docs`: 文档更新
- `style`: 代码格式
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建、工具等

示例:
```
feat(asr): add support for new SenseVoice model

- Add model configuration parsing
- Update inference pipeline  
- Add corresponding unit tests

Closes #123
```

## 🤝 致谢

- [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) - 优秀的语音识别框架
- [SenseVoice](https://github.com/FunAudioLLM/SenseVoice) - 多语言语音识别模型  
- [WebSocket++](https://github.com/zaphoyd/websocketpp) - C++ WebSocket 库

---

**快速链接**: [安装指南](#安装依赖) | [Docker部署](#docker部署) | [API文档](#websocket接口) | [故障排除](#故障排除) | [开发指南](#开发指南)
