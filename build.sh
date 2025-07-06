#!/bin/bash

# Build script for WebSocket ASR Server

set -e

echo "Building WebSocket ASR Server..."

# Set up environment for sherpa-onnx (if installed in user directory)
if [[ -d "$HOME/.local/lib" && -f "$HOME/.local/lib/libsherpa-onnx-core.a" || -f "$HOME/.local/lib/libsherpa-onnx-core.so" ]]; then
    echo "Setting up environment for user-installed sherpa-onnx..."
    export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"
    export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"
fi

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
echo "  python3 test_websocket_client.py --server ws://localhost:8000/sttRealtime --audio-file examples/test.wav"
