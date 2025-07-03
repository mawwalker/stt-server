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

### sherpa-onnx 安装

**重要：请按照官方教程编译安装 sherpa-onnx**

#### 方法一：使用提供的安装脚本（推荐）

```bash
# 使用自动安装脚本（按照官方教程）
./install_sherpa_onnx.sh

# 查看安装选项
./install_sherpa_onnx.sh --help

# 如果使用 GCC<=10 (如 Ubuntu <= 18.04 或 CentOS<=7)，使用共享库
./install_sherpa_onnx.sh --shared

# 自定义并行编译数量
./install_sherpa_onnx.sh -j 8
```

#### 方法二：手动安装（按官方教程）

```bash
# 1. 克隆 sherpa-onnx 源码
git clone https://github.com/k2-fsa/sherpa-onnx
cd sherpa-onnx
mkdir build
cd build

# 2. 使用官方推荐的配置进行编译
# 默认情况下，构建静态库并使用静态链接
cmake -DCMAKE_BUILD_TYPE=Release ..

# 如果使用 GCC<=10 (如 Ubuntu <= 18.04 或 CentOS<=7)，请使用共享库
# cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ..

# 3. 编译 (使用 -j6 或根据你的CPU核心数调整)
make -j6

# 4. 安装到系统
sudo make install
sudo ldconfig  # 更新动态链接库缓存
```

编译完成后，你会在 `bin` 目录下找到可执行文件 `sherpa-onnx`。

## 编译本项目

在安装好 sherpa-onnx 后，编译本项目：

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

## 快速开始

### 使用 Makefile（推荐）

项目提供了完整的 Makefile，可以简化所有操作：

```bash
# 查看所有可用命令
make help

# 一键设置开发环境（安装依赖 + 下载模型 + 编译 + 测试）
make dev-setup

# 本地编译和运行
make build
make run

# Docker 部署
make docker-build
make docker-run

# 使用 Docker Compose
make compose-up
make compose-logs

# 测试服务
make health-check
make test-audio    # 需要提供测试音频文件
```

## Docker 部署

### 快速开始

使用 Docker 是最简单的部署方式，Docker 构建过程会自动按照官方教程编译安装 sherpa-onnx：

```bash
# 克隆项目
git clone <repository-url>
cd websocket-asr-server

# 构建 Docker 镜像（这会自动编译安装 sherpa-onnx）
./docker-build.sh

# 运行容器
docker run -d --name websocket-asr-server -p 8000:8000 websocket-asr-server:latest

# 检查容器状态
docker ps
docker logs websocket-asr-server
```

### Docker 构建过程

Docker 构建会按照以下步骤进行：

1. **安装系统依赖** - 安装编译工具和必要的库
2. **编译 sherpa-onnx** - 按照官方教程从源码编译安装
   ```bash
   git clone https://github.com/k2-fsa/sherpa-onnx
   cd sherpa-onnx
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make -j6
   make install
   ```
3. **下载语音模型** - 下载 SenseVoice 和 VAD 模型
4. **编译本项目** - 编译 WebSocket ASR 服务器
5. **配置运行环境** - 设置用户权限和健康检查

### 使用 Docker Compose

推荐使用 Docker Compose 进行生产环境部署：

```bash
# 启动服务
docker-compose up -d

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down
```

### Docker 构建选项

```bash
# 构建标准镜像（推荐）
./docker-build.sh

# 构建轻量级镜像
./docker-build.sh --alpine

# 自定义镜像名称和标签
./docker-build.sh --image my-asr-server --tag v1.0

# 使用不同端口
./docker-build.sh --port 9000
```

### 生产环境配置

#### 使用 Nginx 反向代理

启用 Nginx 代理（包含负载均衡和 SSL 支持）：

```bash
# 启动带 Nginx 的完整服务
docker-compose --profile with-proxy up -d
```

#### 性能优化

在 `docker-compose.yml` 中调整资源限制：

```yaml
deploy:
  resources:
    limits:
      memory: 4G      # 根据模型大小调整
      cpus: "4.0"     # 根据 CPU 核心数调整
```

#### 持久化数据

挂载模型和日志目录：

```yaml
volumes:
  - ./assets:/app/assets:ro      # 模型文件（只读）
  - ./logs:/app/logs             # 日志文件
  - ./config:/app/config:ro      # 配置文件（只读）
```

### Docker 健康检查

容器内置健康检查，可通过以下方式监控：

```bash
# 检查容器健康状态
docker ps --format "table {{.Names}}\t{{.Status}}"

# 手动健康检查
curl http://localhost:8000/health
```

## 本地编译安装

如果您更喜欢本地编译，请按照以下步骤：

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

## Docker 测试

### 基础功能测试

```bash
# 检查服务是否启动
curl http://localhost:8000/health

# 使用 Docker 容器内的测试脚本
docker exec -it websocket-asr-server python3 test_websocket_client.py \
    --server ws://localhost:8000/asr \
    --audio-file examples/test.wav \
    --sample-rate 16000
```

### 性能测试

```bash
# 并发连接测试
for i in {1..10}; do
    docker exec -d websocket-asr-server python3 test_websocket_client.py \
        --server ws://localhost:8000/asr \
        --audio-file examples/test.wav &
done
wait

# 内存和CPU使用情况
docker stats websocket-asr-server
```

## 故障排除

### Docker 相关问题

1. **构建失败**：
   ```bash
   # 清理 Docker 缓存
   docker system prune -a
   
   # 重新构建（不使用缓存）
   docker build --no-cache -t websocket-asr-server .
   ```

2. **容器启动失败**：
   ```bash
   # 查看详细日志
   docker logs --details websocket-asr-server
   
   # 进入容器调试
   docker exec -it websocket-asr-server /bin/bash
   ```

3. **端口冲突**：
   ```bash
   # 查看端口占用
   netstat -tlnp | grep :8000
   
   # 使用不同端口
   docker run -p 9000:8000 websocket-asr-server
   ```

4. **模型下载失败**：
   ```bash
   # 手动下载模型到本地，然后挂载
   mkdir -p ./assets
   wget -O assets/model.tar.bz2 <model-url>
   docker run -v ./assets:/app/assets websocket-asr-server
   ```

### 本地编译问题

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

## 部署架构选择

### 1. 单容器部署（开发/测试）

适合开发和小规模测试：

```bash
docker run -d --name asr-server -p 8000:8000 websocket-asr-server:latest
```

### 2. Docker Compose 部署（推荐）

适合生产环境，支持服务编排：

```bash
docker-compose up -d
```

### 3. Kubernetes 部署

适合大规模集群部署：

```yaml
# asr-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: websocket-asr-server
spec:
  replicas: 3
  selector:
    matchLabels:
      app: websocket-asr-server
  template:
    metadata:
      labels:
        app: websocket-asr-server
    spec:
      containers:
      - name: asr-server
        image: websocket-asr-server:latest
        ports:
        - containerPort: 8000
        resources:
          requests:
            memory: "1Gi"
            cpu: "500m"
          limits:
            memory: "2Gi"
            cpu: "2000m"
---
apiVersion: v1
kind: Service
metadata:
  name: websocket-asr-service
spec:
  selector:
    app: websocket-asr-server
  ports:
  - port: 80
    targetPort: 8000
  type: LoadBalancer
```

### 4. 性能监控

使用 Prometheus 和 Grafana 监控服务：

```yaml
# 在 docker-compose.yml 中添加
  prometheus:
    image: prom/prometheus
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml

  grafana:
    image: grafana/grafana
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
```

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

## 贡献指南

### 开发环境设置

```bash
# 1. 克隆项目
git clone <repository-url>
cd websocket-asr-server

# 2. 设置开发环境
make dev-setup

# 3. 进行开发
# 编辑代码...

# 4. 测试更改
make build
make test

# 5. 格式化代码
make format

# 6. 构建 Docker 镜像测试
make docker-build
```

### 代码风格

- 使用 clang-format 格式化 C++ 代码
- 遵循 Google C++ Style Guide
- 添加适当的注释和文档
- 编写单元测试

### 提交流程

1. Fork 项目
2. 创建功能分支: `git checkout -b feature/amazing-feature`
3. 提交更改: `git commit -m 'Add amazing feature'`
4. 推送到分支: `git push origin feature/amazing-feature`
5. 创建 Pull Request

### CI/CD

项目使用 GitHub Actions 进行持续集成：

- 自动构建和测试 (Ubuntu)
- Docker 镜像构建测试
- 代码风格检查
- 基本功能测试

查看 `.github/workflows/build-and-test.yml` 了解详细配置。

## 项目结构

```
├── CMakeLists.txt              # CMake 配置文件（已优化）
├── build.sh                    # 构建脚本
├── install_sherpa_onnx.sh      # sherpa-onnx 自动安装脚本
├── docker-build.sh             # Docker 构建脚本
├── Dockerfile                  # Docker 配置（标准版）
├── Dockerfile.alpine           # Docker 配置（轻量版）
├── docker-compose.yml          # Docker Compose 配置
├── main.cpp                    # 主程序入口
├── include/                    # 头文件目录
│   ├── asr_engine.h           # ASR 引擎接口
│   ├── asr_session.h          # ASR 会话管理
│   ├── websocket_server.h     # WebSocket 服务器
│   └── ...                    # 其他头文件
├── src/                       # 源文件目录
│   ├── asr_engine.cpp         # ASR 引擎实现
│   ├── asr_session.cpp        # ASR 会话实现
│   ├── websocket_server.cpp   # WebSocket 服务器实现
│   └── logger.cpp             # 日志系统
├── assets/                    # 模型文件目录
│   ├── sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/
│   └── silero_vad/
└── examples/                  # 示例和测试文件
    └── test_websocket_client.py
```

### 项目优化说明

本项目已按照 sherpa-onnx 官方教程进行了以下优化：

1. **移除了内置的 sherpa-onnx 副本** - 不再在 `include/sherpa-onnx/` 目录下包含 sherpa-onnx 源码
2. **优化了 CMakeLists.txt** - 使用系统安装的 sherpa-onnx 库，支持多种查找方式
3. **提供了自动安装脚本** - `install_sherpa_onnx.sh` 按照官方教程自动编译安装 sherpa-onnx
4. **优化了 Docker 构建** - Docker 构建过程严格按照官方教程编译 sherpa-onnx
5. **简化了依赖管理** - 先安装 sherpa-onnx，再编译本项目，避免复杂的依赖处理

## 许可证

本项目遵循与 sherpa-onnx 相同的许可证。

## 快速开始指南

### 方法一：一键安装（最简单）

```bash
# 1. 克隆项目
git clone <repository-url>
cd websocket-asr-server

# 2. 一键安装所有依赖、编译sherpa-onnx、下载模型、编译项目
chmod +x setup.sh
./setup.sh

# 3. 运行服务
./build/websocket_asr_server --models-root ./assets --port 8000 --threads 2
```

**一键安装脚本会自动完成：**
- 安装系统依赖 (cmake, gcc, jsoncpp 等)
- 按照官方教程编译安装 sherpa-onnx
- 下载语音识别模型
- 编译本项目
- 验证安装

### 方法二：分步安装

```bash
# 1. 克隆项目
git clone <repository-url>
cd websocket-asr-server

# 2. 安装系统依赖
chmod +x install_deps.sh
./install_deps.sh

# 3. 安装 sherpa-onnx
chmod +x install_sherpa_onnx.sh
./install_sherpa_onnx.sh

# 4. 编译项目
./build.sh

# 5. 下载模型（如果还没有）
# 下载 SenseVoice 模型
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
tar xvf sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
mv sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17 assets/

# 下载 VAD 模型
mkdir -p assets/silero_vad
wget -O assets/silero_vad/silero_vad.onnx \
    https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx

# 6. 运行服务
./build/websocket_asr_server --models-root ./assets --port 8000 --threads 2
```

### 方法三：使用 Docker（无需安装依赖）

```bash
# 1. 克隆项目
git clone <repository-url>
cd websocket-asr-server

# 2. 构建并运行（一键完成）
./docker-build.sh
docker run -p 8000:8000 websocket-asr-server:latest
```
