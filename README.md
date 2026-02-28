# C++ WebSocket ASR Server

基于 sherpa-onnx 和 SenseVoice 的 C++ WebSocket 流式语音识别服务器。

## 📋 目录导航

- [功能特性](#功能特性)
- [快速开始](#快速开始)
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
- **🆕 OneShot一句话识别** - 完整音频录制后进行一次性识别，支持语言检测、情感分析
- **双端点支持** - `/sttRealtime`(流式) + `/oneshot`(一句话)
- **WebSocket 接口** - 兼容现有的 Python 客户端，支持两种识别模式
- **多语言支持** - 中文、英文、日文、韩文、粤语
- **内置 VAD** - Silero VAD 语音活动检测
- **高并发** - 支持多客户端同时连接
- **低延迟** - C++ 实现，性能优异
- **容器化部署** - 完整的 Docker 解决方案
## ⚡ 快速开始

### 1. 准备项目
```bash
# 克隆项目并进入目录
git clone <your-repository-url>
cd stt
```

### 2. 构建与运行

**本地构建运行：**
```bash
./build.sh
./build/websocket_asr_server --models-root ./assets --port 8000
```

**Docker构建运行（推荐）：**
```bash
./docker_build.sh
docker run -d --name asr-server -p 8000:8000 websocket-asr-server:latest
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

## 🌐 WebSocket接口

### 接口端点

**流式识别**: `ws://localhost:8000/sttRealtime?samplerate=16000`
- 实时流式语音识别，边说边识别
- 适用于实时对话、语音助手等场景

**OneShot一句话识别**: `ws://localhost:8000/oneshot`
- 完整音频录制后一次性识别，支持语言检测、情感分析
- 适用于音频文件转写、语音命令识别等场景
- 详细文档：[OneShot识别指南](ONESHOT_GUIDE.md)

### 客户端使用

#### 快速测试

```bash
# 流式识别 - 音频文件
python websocket_client.py --mode streaming --file examples/test.mp3

# 流式识别 - 麦克风（持续录音）
python websocket_client.py --mode streaming --mic

# OneShot识别 - 音频文件（一次性处理）
python websocket_client.py --mode oneshot --file examples/test.mp3

# OneShot识别 - 麦克风（录音5秒后识别）
python websocket_client.py --mode oneshot --mic --duration 5

# 交互式测试（对比两种模式）
python oneshot_examples.py
```

#### 参数说明

- `--mode`: 识别模式 (`streaming`|`oneshot`)
- `--file`: 音频文件路径
- `--mic`: 使用麦克风输入
- `--duration`: 录音时长（秒）
- `--sample-rate`: 音频采样率（默认16000）

### 通信协议

#### 流式识别协议

**连接**: `ws://localhost:8000/sttRealtime?samplerate=16000`
**发送**: 二进制音频数据（16-bit PCM）
**接收**: JSON格式结果

```json
{
    "text": "识别的文本",
    "finished": false,    // true=最终结果，false=部分结果
    "idx": 0,            // 语音段索引  
    "lang": "zh"         // 语言代码
}
```

#### OneShot识别协议

**连接**: `ws://localhost:8000/oneshot`

**发送控制消息**:
```json
{"command": "start"}    // 开始录音
{"command": "stop"}     // 停止录音并处理
```

**发送音频**: 二进制音频数据（16-bit PCM）

**接收消息**:
```json
// 状态消息
{
    "type": "status",
    "status": "ready"    // ready|recording|processing|finished
}

// 识别结果（包含更多元数据）
{
    "type": "result", 
    "text": "识别的文本",
    "finished": true,
    "idx": 0,
    "lang": "zh",           // 检测到的语言
    "emotion": "neutral",   // 情感信息
    "event": "",           // 事件信息
    "timestamps": [...]    // 时间戳数组
}

// 错误消息
{
    "type": "error",
    "message": "错误描述"
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
    uri = "ws://localhost:8000/sttRealtime?samplerate=16000"
    
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
const socket = new WebSocket('ws://localhost:8000/sttRealtime?samplerate=16000');

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
