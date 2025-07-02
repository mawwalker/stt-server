#!/bin/bash

# Build script for WebSocket ASR Server

set -e

echo "Building WebSocket ASR Server..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17

# Build
make -j$(nproc)

echo "Build completed successfully!"
echo "Executable: build/websocket_asr_server"
echo ""
echo "Usage:"
echo "  ./build/websocket_asr_server --models-root ./models --port 8000 --threads 2"
echo ""
echo "Test with:"
echo "  python3 test_websocket_client.py --server ws://localhost:8000/asr --audio-file examples/test.wav"
