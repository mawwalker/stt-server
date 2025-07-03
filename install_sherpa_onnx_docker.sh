#!/bin/bash

# Docker optimized sherpa-onnx installation script
# Uses unified configuration from sherpa_config.sh
# Installs to /usr/local for system-wide access in container

set -e

echo "Installing sherpa-onnx to /usr/local with optimized build for Docker..."

# Load unified configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/sherpa_config.sh"

# Check GCC version for compatibility
if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
    echo "GCC version: $GCC_VERSION"
else
    echo "GCC not found, using default build settings"
    GCC_VERSION=11  # Assume modern GCC
fi

# Use temporary directory for build
BUILD_DIR="/tmp/sherpa-onnx-build"
INSTALL_DIR="/usr/local"
echo "Building in: $BUILD_DIR"
echo "Installing to: $INSTALL_DIR"

# Clean up any existing build
if [[ -d "$BUILD_DIR" ]]; then
    rm -rf "$BUILD_DIR"
fi

# Clone repository
echo "Cloning sherpa-onnx repository..."
git clone https://github.com/k2-fsa/sherpa-onnx "$BUILD_DIR"
cd "$BUILD_DIR"
mkdir build
cd build

echo "Configuring sherpa-onnx with minimal dependencies for faster Docker build..."
show_config_info

# Generate CMake options using unified configuration
SHARED_LIBS_OPTION=$(get_shared_libs_option)
CMAKE_OPTIONS=$(generate_cmake_options "$INSTALL_DIR" "$SHARED_LIBS_OPTION")

if [[ "$SHARED_LIBS_OPTION" == "ON" ]]; then
    echo "Using shared libraries build (GCC <= 10)"
else
    echo "Using static libraries build (default)"
fi

echo "Running CMake with options: $CMAKE_OPTIONS"
cmake $CMAKE_OPTIONS ..

echo "Building sherpa-onnx with $(nproc) parallel jobs..."
# Build and install to system directory
make -j$(nproc)
make install

echo "sherpa-onnx optimized installation completed!"
echo "📦 Installed to: $INSTALL_DIR/lib and $INSTALL_DIR/include"

echo "Cleaning up build directory..."
cd /
rm -rf "$BUILD_DIR"
echo "Done!"
