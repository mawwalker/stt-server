#!/bin/bash

# Environment setup script for sherpa-onnx
# Source this script to set up environment variables

# Set up library and include paths for sherpa-onnx installed in user directory
export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH="$HOME/.local/bin:$PATH"

echo "Environment variables set for sherpa-onnx:"
echo "  LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo "  PKG_CONFIG_PATH: $PKG_CONFIG_PATH"
echo "  PATH: $PATH"
echo ""
echo "You can now build your project with: ./build.sh"
