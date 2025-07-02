# C++ WebSocket ASR Server

基于 sherpa-onnx 和 SenseVoice 的 C++ WebSocket 流式语音识别服务器。

## 功能特性

- 实时流式语音识别
- WebSocket 接口，兼容现有的 Python 客户端
- 支持多语言识别（中文、英文、日文、韩文、粤语）
- 内置 VAD（语音活动检测）
- 支持并发连接
- 低延迟推理

## 依赖项

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

### sherpa-onnx
需要安装 sherpa-onnx 库：
```bash
# 从源码编译安装 sherpa-onnx
git clone https://github.com/k2-fsa/sherpa-onnx.git
cd sherpa-onnx
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

## 编译

```bash
# 给构建脚本执行权限
chmod +x build.sh

# 编译项目
./build.sh
```

## 模型准备

确保在 `assets` 目录下有以下模型文件：

```
assets/
├── sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/
│   ├── model.onnx
│   └── tokens.txt
└── silero_vad/
    └── silero_vad.onnx
```

如果没有模型文件，可以下载：
```bash
# 下载 SenseVoice 模型
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
tar xvf sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
mv sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17 assets/

# 下载 VAD 模型
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx
mkdir -p assets/silero_vad
mv silero_vad.onnx assets/silero_vad/
```

## 使用方法

### 启动服务器

```bash
# 基本启动
./build/websocket_asr_server

# 自定义参数
./build/websocket_asr_server \
    --models-root ./assets \
    --port 8000 \
    --threads 4
```

### 服务器参数

- `--port PORT`: 服务器端口（默认：8000）
- `--models-root PATH`: 模型文件目录（默认：./assets）
- `--threads NUM`: 推理线程数（默认：2）
- `--help`: 显示帮助信息

### WebSocket 接口

**连接地址：** `ws://localhost:8000/asr?samplerate=16000`

**协议：**
- 发送：PCM 音频数据（16位，单声道，指定采样率）
- 接收：JSON 格式的识别结果

**响应格式：**
```json
{
    "text": "识别的文本",
    "finished": false,  // true表示最终结果，false表示部分结果
    "idx": 0           // 语音段索引
}
```

## 测试

### 使用音频文件测试

```bash
# 安装 Python 依赖
pip install websockets wave

# 测试音频文件
python test_websocket_client.py \
    --server ws://localhost:8000/asr \
    --audio-file examples/test.wav \
    --sample-rate 16000
```

### 使用麦克风测试

```bash
# 安装额外依赖
pip install pyaudio

# 测试麦克风输入
python test_websocket_client.py \
    --server ws://localhost:8000/asr \
    --microphone \
    --duration 10
```

### 与 Python 服务对比

启动 Python 服务：
```bash
python app.py --port 8001
```

启动 C++ 服务：
```bash
./build/websocket_asr_server --port 8000
```

使用相同的测试文件对比性能和准确性。

## 性能优化

### 编译优化
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"
make -j$(nproc)
```

### 运行时优化
- 增加推理线程数：`--threads 4`
- 确保模型文件在快速存储设备上
- 使用 GPU 推理（如果 sherpa-onnx 支持）

## 架构说明

### 主要组件

1. **ASREngine**: 管理 sherpa-onnx 推理引擎和 VAD
2. **ASRSession**: 处理单个 WebSocket 连接的音频流
3. **WebSocketASRServer**: WebSocket 服务器，管理所有连接

### 处理流程

1. 客户端连接 WebSocket
2. 创建 ASRSession 实例
3. 接收 PCM 音频数据
4. VAD 检测语音活动
5. 流式推理生成部分结果
6. VAD 检测到语音结束，生成最终结果
7. 通过 WebSocket 发送 JSON 结果

### 与 Python 版本的差异

- **性能**: C++ 版本具有更低的延迟和更高的吞吐量
- **内存**: 更高效的内存使用
- **并发**: 原生多线程支持
- **部署**: 单一可执行文件，无需 Python 环境

## 故障排除

### 编译错误

1. **找不到 sherpa-onnx**：
   ```bash
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   sudo ldconfig
   ```

2. **找不到 websocketpp**：
   ```bash
   sudo apt-get install libwebsocketpp-dev
   ```

3. **找不到 jsoncpp**：
   ```bash
   sudo apt-get install libjsoncpp-dev
   ```

### 运行时错误

1. **模型文件不存在**：
   - 检查模型路径：`--models-root ./assets`
   - 确保模型文件完整下载

2. **端口被占用**：
   - 使用不同端口：`--port 8001`
   - 检查端口占用：`netstat -tlnp | grep :8000`

3. **连接失败**：
   - 检查防火墙设置
   - 确认服务器正常启动
   - 验证 WebSocket URL 格式

## 开发和扩展

### 添加新模型支持

修改 `ASREngine::initialize()` 方法，添加新的模型配置。

### 自定义 VAD 参数

在 `ASREngine::initialize()` 中调整 VAD 配置：
```cpp
vad_config.silero_vad.threshold = 0.5;
vad_config.silero_vad.min_silence_duration = 0.25;
```

### 添加认证

在 WebSocket 连接处理中添加认证逻辑。

## 许可证

本项目遵循与 sherpa-onnx 相同的许可证。
