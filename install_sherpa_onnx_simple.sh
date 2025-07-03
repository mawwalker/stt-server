#!/bin/bash

# Simple sherpa-onnx installation script
# Uses unified configuration from sherpa_config.sh
# Installs to user directory (~/.local)

set -e

echo "Installing sherpa-onnx to user directory with minimal dependencies..."

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
USER_INSTALL_DIR="$HOME/.local"
echo "Building in: $BUILD_DIR"
echo "Installing to: $USER_INSTALL_DIR"

# Ensure user local directories exist
mkdir -p "$USER_INSTALL_DIR/lib"
mkdir -p "$USER_INSTALL_DIR/include"
mkdir -p "$USER_INSTALL_DIR/bin"

# Clean up any existing build
if [[ -d "$BUILD_DIR" ]]; then
    rm -rf "$BUILD_DIR"
fi

# Clone repository
git clone https://github.com/k2-fsa/sherpa-onnx "$BUILD_DIR"
cd "$BUILD_DIR"
mkdir build
cd build

echo "Configuring sherpa-onnx with minimal dependencies for faster compilation..."
show_config_info

# Generate CMake options using unified configuration
SHARED_LIBS_OPTION=$(get_shared_libs_option)
CMAKE_OPTIONS=$(generate_cmake_options "$USER_INSTALL_DIR" "$SHARED_LIBS_OPTION")

if [[ "$SHARED_LIBS_OPTION" == "ON" ]]; then
    echo "Using shared libraries build (GCC <= 10)"
else
    echo "Using static libraries build (default)"
fi

echo "Running CMake with options: $CMAKE_OPTIONS"
cmake $CMAKE_OPTIONS ..

# Build and install to user directory
make -j6
make install

# Update LD_LIBRARY_PATH for current session
export LD_LIBRARY_PATH="$USER_INSTALL_DIR/lib:$LD_LIBRARY_PATH"

echo "sherpa-onnx optimized installation completed!"
echo "Installed to: $USER_INSTALL_DIR/lib and $USER_INSTALL_DIR/include"
echo ""
show_config_info
echo ""
echo "To use sherpa-onnx in future sessions, add the following to your ~/.bashrc or ~/.zshrc:"
echo "export LD_LIBRARY_PATH=\"\$HOME/.local/lib:\$LD_LIBRARY_PATH\""
echo "export PKG_CONFIG_PATH=\"\$HOME/.local/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo "Cleaning up build directory..."
cd /
rm -rf "$BUILD_DIR"
echo "Done!"
