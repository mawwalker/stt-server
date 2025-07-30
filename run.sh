#!/bin/bash
# WebSocket ASR Server 统一启动脚本
# 支持本地和Docker环境的统一启动方式

set -e

# 脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 检测并设置Docker Compose命令
detect_docker_compose() {
    if command -v "docker" >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
        echo "docker compose"
    elif command -v "docker-compose" >/dev/null 2>&1; then
        echo "docker-compose"
    else
        echo ""
    fi
}

# 设置Docker Compose命令
DOCKER_COMPOSE_CMD=$(detect_docker_compose)

# 加载环境变量
if [ -f ".env" ]; then
    echo "Loading configuration from .env file..."
    export $(grep -v '^#' .env | grep -v '^$' | xargs)
fi

# 显示帮助信息
show_help() {
    cat << EOF
WebSocket ASR Server 统一启动脚本

用法:
    $0 [命令] [选项]

命令:
    local       本地启动（直接运行二进制文件）
    docker      Docker启动（使用Docker Compose）
    build       编译项目
    clean       清理构建文件
    stop        停止Docker服务
    logs        查看Docker日志
    status      查看服务状态
    help        显示此帮助信息

选项:
    --env-file FILE     指定环境文件（默认: .env）
    --port PORT         指定服务端口（覆盖环境变量）
    --log-level LEVEL   指定日志级别（覆盖环境变量）
    --background        后台运行（仅适用于local模式）
    --rebuild           强制重新构建Docker镜像

示例:
    $0 local                           # 本地启动
    $0 local --port 9000               # 本地启动，使用端口9000
    $0 docker                          # Docker启动
    $0 docker --rebuild                # 重新构建并启动
    $0 build                           # 编译项目
    $0 logs                            # 查看Docker日志

EOF
}

# 检查二进制文件是否存在
check_binary() {
    if [ ! -f "build/websocket_asr_server" ]; then
        echo "Error: websocket_asr_server binary not found in build/"
        echo "Please run '$0 build' first to compile the project."
        exit 1
    fi
}

# 编译项目
build_project() {
    echo "Building WebSocket ASR Server..."
    
    if [ ! -f "CMakeLists.txt" ]; then
        echo "Error: CMakeLists.txt not found"
        exit 1
    fi
    
    # 创建构建目录
    mkdir -p build
    cd build
    
    # 配置和编译
    cmake ..
    make -j$(nproc)
    
    echo "Build completed successfully!"
    echo "Binary location: build/websocket_asr_server"
}

# 本地启动
start_local() {
    echo "Starting WebSocket ASR Server locally..."
    
    check_binary
    
    # 构建启动命令
    cmd="./build/websocket_asr_server"
    
    # 添加命令行参数（如果有的话）
    while [[ $# -gt 0 ]]; do
        case $1 in
            --port)
                export SERVER_PORT="$2"
                shift 2
                ;;
            --log-level)
                export LOG_LEVEL="$2"
                shift 2
                ;;
            --background)
                BACKGROUND=true
                shift
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    echo "Configuration:"
    echo "  Server Port: ${SERVER_PORT:-8000}"
    echo "  Models Root: ${MODELS_ROOT:-./assets}"
    echo "  Log Level: ${LOG_LEVEL:-INFO}"
    echo ""
    
    # 启动服务
    if [ "$BACKGROUND" = "true" ]; then
        echo "Starting server in background..."
        nohup $cmd > server.log 2>&1 &
        echo "Server started with PID: $!"
        echo "Log file: server.log"
        echo "To stop: kill $!"
    else
        echo "Starting server..."
        echo "Press Ctrl+C to stop"
        exec $cmd
    fi
}

# Docker启动
start_docker() {
    echo "Starting WebSocket ASR Server with Docker..."
    
    # 检查Docker Compose命令是否可用
    if [ -z "$DOCKER_COMPOSE_CMD" ]; then
        echo "Error: Neither 'docker compose' nor 'docker-compose' is available."
        echo "Please install Docker Compose first."
        exit 1
    fi
    
    echo "Using Docker Compose command: $DOCKER_COMPOSE_CMD"
    
    local REBUILD=false
    
    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --rebuild)
                REBUILD=true
                shift
                ;;
            --env-file)
                ENV_FILE="$2"
                shift 2
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # 检查docker-compose文件
    if [ ! -f "docker-compose.yml" ]; then
        echo "Error: docker-compose.yml not found"
        exit 1
    fi
    
    # 构建Docker镜像（如果需要）
    if [ "$REBUILD" = "true" ]; then
        echo "Rebuilding Docker image..."
        $DOCKER_COMPOSE_CMD build --no-cache
    fi
    
    # 启动服务
    echo "Starting Docker services..."
    $DOCKER_COMPOSE_CMD --env-file "${ENV_FILE:-.env}" up -d
    
    echo "Docker services started!"
    echo "Server URL: http://localhost:${SERVER_PORT:-8000}"
    echo ""
    echo "Useful commands:"
    echo "  $0 logs     # 查看日志"
    echo "  $0 status   # 查看状态"
    echo "  $0 stop     # 停止服务"
}

# 停止Docker服务
stop_docker() {
    echo "Stopping Docker services..."
    
    # 检查Docker Compose命令是否可用
    if [ -z "$DOCKER_COMPOSE_CMD" ]; then
        echo "Error: Neither 'docker compose' nor 'docker-compose' is available."
        exit 1
    fi
    
    $DOCKER_COMPOSE_CMD down
    echo "Docker services stopped."
}

# 查看Docker日志
show_logs() {
    # 检查Docker Compose命令是否可用
    if [ -z "$DOCKER_COMPOSE_CMD" ]; then
        echo "Error: Neither 'docker compose' nor 'docker-compose' is available."
        exit 1
    fi
    
    if [ ! "$($DOCKER_COMPOSE_CMD ps -q)" ]; then
        echo "No running Docker services found."
        exit 1
    fi
    
    echo "Showing Docker logs (Press Ctrl+C to exit)..."
    $DOCKER_COMPOSE_CMD logs -f
}

# 查看服务状态
show_status() {
    echo "=== Local Binary Status ==="
    if [ -f "build/websocket_asr_server" ]; then
        echo "✓ Binary exists: build/websocket_asr_server"
        
        # 检查是否在运行
        if pgrep -f "websocket_asr_server" > /dev/null; then
            echo "✓ Local server is running"
            echo "  PID: $(pgrep -f websocket_asr_server)"
            echo "  Port: $(netstat -tlnp 2>/dev/null | grep :${SERVER_PORT:-8000} | head -1 || echo 'Not found')"
        else
            echo "✗ Local server is not running"
        fi
    else
        echo "✗ Binary not found (run '$0 build' first)"
    fi
    
    echo ""
    echo "=== Docker Status ==="
    if [ -n "$DOCKER_COMPOSE_CMD" ]; then
        echo "✓ Docker Compose available: $DOCKER_COMPOSE_CMD"
        if [ -f "docker-compose.yml" ]; then
            echo "✓ docker-compose.yml exists"
            
            # 检查Docker服务状态
            if $DOCKER_COMPOSE_CMD ps | grep -q "Up"; then
                echo "✓ Docker services are running:"
                $DOCKER_COMPOSE_CMD ps
            else
                echo "✗ Docker services are not running"
                echo "  Run '$0 docker' to start"
            fi
        else
            echo "✗ docker-compose.yml not found"
        fi
    else
        echo "✗ Docker Compose not available"
        echo "  Neither 'docker compose' nor 'docker-compose' found"
    fi
    
    echo ""
    echo "=== Configuration ==="
    echo "Environment file: ${ENV_FILE:-.env}"
    if [ -f ".env" ]; then
        echo "✓ .env file exists"
        echo "Key settings:"
        echo "  SERVER_PORT=${SERVER_PORT:-8000}"
        echo "  LOG_LEVEL=${LOG_LEVEL:-INFO}"
        echo "  ASR_MODEL_NAME=${ASR_MODEL_NAME:-sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17}"
    else
        echo "✗ .env file not found"
    fi
}

# 清理构建文件
clean_build() {
    echo "Cleaning build files..."
    if [ -d "build" ]; then
        rm -rf build/*
        echo "Build directory cleaned."
    else
        echo "Build directory doesn't exist."
    fi
}

# 主程序逻辑
main() {
    case "${1:-help}" in
        local)
            shift
            start_local "$@"
            ;;
        docker)
            shift
            start_docker "$@"
            ;;
        build)
            build_project
            ;;
        clean)
            clean_build
            ;;
        stop)
            stop_docker
            ;;
        logs)
            show_logs
            ;;
        status)
            show_status
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            echo "Unknown command: $1"
            echo "Run '$0 help' for usage information."
            exit 1
            ;;
    esac
}

# 执行主程序
main "$@"
