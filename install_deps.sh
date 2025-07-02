#!/bin/bash

# Install dependencies for WebSocket ASR Server

echo "Installing dependencies..."

# Update system packages
sudo apt update

# Install JSON library
sudo apt install -y libjsoncpp-dev

# Install WebSocket++ dependencies
sudo apt install -y libboost-all-dev

# Install build tools if not already installed
sudo apt install -y build-essential cmake pkg-config

echo "Dependencies installed successfully!"

# Check if sherpa-onnx is installed
if ! pkg-config --exists sherpa-onnx; then
    echo "Warning: sherpa-onnx not found via pkg-config"
    echo "Please make sure sherpa-onnx is properly installed"
else
    echo "sherpa-onnx found via pkg-config"
fi

echo "You can now build the project with:"
echo "mkdir -p build && cd build"
echo "cmake .."
echo "make -j$(nproc)"
