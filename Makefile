# WebSocket ASR Server Makefile
# 提供简化的构建和运行命令

.PHONY: help build clean run run-local run-docker stop logs status install-deps

# 默认目标
.DEFAULT_GOAL := help

# 加载环境变量
include .env
export

# 帮助信息
help: ## 显示帮助信息
	@echo "WebSocket ASR Server - 构建和运行命令"
	@echo ""
	@echo "基础命令:"
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "  %-15s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo ""
	@echo "示例:"
	@echo "  make build         # 编译项目"
	@echo "  make run           # 本地运行（默认）"
	@echo "  make run-docker    # Docker运行"
	@echo "  make logs          # 查看Docker日志"
	@echo ""
	@echo "或者使用统一脚本:"
	@echo "  ./run.sh help      # 查看完整帮助"

# 构建项目
build: ## 编译项目
	@echo "Building WebSocket ASR Server..."
	@mkdir -p build
	@cd build && cmake .. && make -j$$(nproc)
	@echo "Build completed: build/websocket_asr_server"

# 清理构建文件
clean: ## 清理构建文件
	@echo "Cleaning build files..."
	@rm -rf build/*
	@echo "Build directory cleaned."

# 本地运行（默认）
run: run-local ## 本地运行服务

# 本地运行
run-local: build ## 编译并本地运行服务
	@echo "Starting WebSocket ASR Server locally..."
	@echo "Server Port: $(SERVER_PORT)"
	@echo "Models Root: $(MODELS_ROOT)"
	@echo "Log Level: $(LOG_LEVEL)"
	@echo "Press Ctrl+C to stop"
	@./build/websocket_asr_server

# 后台运行本地服务
run-background: build ## 后台运行本地服务
	@echo "Starting WebSocket ASR Server in background..."
	@nohup ./build/websocket_asr_server > server.log 2>&1 &
	@echo "Server started with PID: $$!"
	@echo "Log file: server.log"

# Docker运行
run-docker: ## 使用Docker运行服务
	@echo "Starting WebSocket ASR Server with Docker..."
	@docker-compose --env-file .env up -d
	@echo "Docker services started!"
	@echo "Server URL: http://localhost:$(SERVER_PORT)"

# 重新构建并运行Docker
rebuild-docker: ## 重新构建Docker镜像并运行
	@echo "Rebuilding and starting with Docker..."
	@docker-compose build --no-cache
	@docker-compose --env-file .env up -d
	@echo "Docker services rebuilt and started!"

# 停止Docker服务
stop: ## 停止Docker服务
	@docker-compose down
	@echo "Docker services stopped."

# 查看Docker日志
logs: ## 查看Docker日志
	@docker-compose logs -f

# 查看服务状态
status: ## 查看服务状态
	@./run.sh status

# 安装依赖（如果需要）
install-deps: ## 安装系统依赖
	@echo "Installing system dependencies..."
	@echo "Please install the following manually if not already installed:"
	@echo "  - CMake (>=3.10)"
	@echo "  - GCC/G++ compiler"
	@echo "  - Docker and Docker Compose"
	@echo "  - sherpa-onnx library"

# 快速启动（检测环境）
quick-start: ## 智能选择启动方式
	@if command -v docker-compose >/dev/null 2>&1 && [ -f "docker-compose.yml" ]; then \
		echo "Docker available, starting with Docker..."; \
		make run-docker; \
	else \
		echo "Docker not available, starting locally..."; \
		make run-local; \
	fi

# 开发模式（实时编译和运行）
dev: ## 开发模式（监控文件变化并重新编译）
	@echo "Development mode - watching for changes..."
	@echo "Press Ctrl+C to stop"
	@while true; do \
		make build && make run-background; \
		sleep 2; \
		inotifywait -qq -r -e modify,create,delete src/ include/ CMakeLists.txt 2>/dev/null || true; \
		pkill -f websocket_asr_server 2>/dev/null || true; \
		sleep 1; \
	done

# 测试（如果有测试）
test: build ## 运行测试
	@echo "Running tests..."
	@if [ -f "build/test_websocket_asr_server" ]; then \
		./build/test_websocket_asr_server; \
	else \
		echo "No tests found. Build with tests enabled if available."; \
	fi

# 格式化代码（如果有clang-format）
format: ## 格式化代码
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "Formatting code..."; \
		find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "Code formatted."; \
	else \
		echo "clang-format not found, skipping formatting."; \
	fi

# 打包发布
package: build ## 创建发布包
	@echo "Creating release package..."
	@mkdir -p release
	@cp build/websocket_asr_server release/
	@cp .env.example release/ 2>/dev/null || cp .env release/
	@cp README.md release/ 2>/dev/null || echo "# WebSocket ASR Server" > release/README.md
	@tar -czf websocket-asr-server-$$(date +%Y%m%d).tar.gz -C release .
	@echo "Package created: websocket-asr-server-$$(date +%Y%m%d).tar.gz"
