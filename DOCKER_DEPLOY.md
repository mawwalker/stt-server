# Docker 部署指南

本文档介绍如何使用 Docker 构建和部署 WebSocket ASR Server。

## 快速开始

### 1. 构建 Docker 镜像

```bash
# 在项目根目录下执行
docker build -t websocket-asr-server .
```

### 2. 运行容器

#### 方法一：使用 docker run
```bash
docker run -d \
  --name websocket-asr-server \
  -p 8000:8000 \
  -e LOG_LEVEL=INFO \
  websocket-asr-server
```

#### 方法二：使用 docker-compose (推荐)
```bash
# 构建并启动
docker-compose up -d

# 查看日志
docker-compose logs -f websocket-asr-server

# 停止服务
docker-compose down
```

## 配置选项

### 环境变量

| 变量名 | 默认值 | 说明 |
|--------|--------|------|
| `MODELS_ROOT` | `/app/assets` | 模型文件根目录 |
| `PORT` | `8000` | 服务监听端口 |
| `THREADS` | `2` | 处理线程数 |
| `LOG_LEVEL` | `INFO` | 日志级别 (DEBUG/INFO/WARN/ERROR) |

### 端口映射

默认容器监听 8000 端口，可以通过以下方式修改：

```bash
# 映射到主机的 9000 端口
docker run -p 9000:8000 websocket-asr-server
```

### 数据卷挂载

#### 挂载本地模型文件
```bash
docker run -v ./assets:/app/assets:ro websocket-asr-server
```

#### 持久化日志
```bash
docker run -v ./logs:/app/logs websocket-asr-server
```

## 高级配置

### 资源限制

```bash
docker run \
  --cpus="2.0" \
  --memory="4g" \
  websocket-asr-server
```

### 自定义构建参数

如果您需要修改构建参数，可以编辑 Dockerfile 或使用构建参数：

```bash
# 使用不同的基础镜像
docker build --build-arg BASE_IMAGE=ubuntu:20.04 -t websocket-asr-server .
```

## 故障排除

### 1. 检查容器状态
```bash
docker ps -a
docker logs websocket-asr-server
```

### 2. 进入容器调试
```bash
docker exec -it websocket-asr-server /bin/bash
```

### 3. 检查模型文件
```bash
docker exec websocket-asr-server ls -la /app/assets
```

### 4. 测试服务
```bash
# 检查健康状态
curl http://localhost:8000/health

# 使用示例客户端测试
docker exec websocket-asr-server python3 /app/examples/test.py
```

## 性能优化

### 1. 多核心利用
增加 `THREADS` 环境变量值以利用更多 CPU 核心：
```bash
docker run -e THREADS=4 websocket-asr-server
```

### 2. 内存优化
为容器分配足够的内存：
```bash
docker run --memory="8g" websocket-asr-server
```

### 3. 模型预加载
确保模型文件在容器启动时就已存在，避免运行时下载。

## 生产环境部署

### 1. 使用 Docker Swarm
```bash
docker service create \
  --name websocket-asr-server \
  --replicas 3 \
  --publish 8000:8000 \
  websocket-asr-server
```

### 2. 使用 Kubernetes
参考 `k8s/` 目录下的配置文件。

### 3. 负载均衡
建议在多个容器实例前放置负载均衡器，如 Nginx 或 HAProxy。

## 维护

### 1. 更新镜像
```bash
# 重新构建
docker build -t websocket-asr-server:latest .

# 滚动更新
docker-compose up -d --force-recreate
```

### 2. 清理资源
```bash
# 清理未使用的镜像
docker image prune -f

# 清理所有相关资源
docker-compose down --rmi all --volumes
```
