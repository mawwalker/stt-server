#!/bin/bash

# Install sherpa-onnx dependencies
# Based on official tutorial: https://github.com/k2-fsa/sherpa-onnx

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root (not needed for user directory install)
check_root() {
    if [[ $EUID -eq 0 ]]; then
        print_warning "Running as root is not recommended for user directory installation."
        print_warning "Consider running as regular user for ~/.local installation."
    else
        print_info "Installing to user directory ~/.local (no sudo required)"
    fi
}

# Check GCC version
check_gcc_version() {
    if command -v gcc &> /dev/null; then
        GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
        print_info "Detected GCC version: $GCC_VERSION"
        
        if [[ $GCC_VERSION -le 10 ]]; then
            print_warning "GCC version <= 10 detected. Will build shared libraries to avoid link errors."
            USE_SHARED_LIBS=ON
        else
            print_info "GCC version > 10. Will build static libraries (default)."
            USE_SHARED_LIBS=OFF
        fi
    else
        print_warning "GCC not found. Using default build configuration."
        USE_SHARED_LIBS=OFF
    fi
}

# Check if cmake is available
check_cmake() {
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is not installed. Please install cmake first."
        print_info "Ubuntu/Debian: sudo apt-get install cmake"
        print_info "CentOS/RHEL: sudo yum install cmake"
        print_info "Arch: sudo pacman -S cmake"
        exit 1
    fi
    
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    print_info "CMake version: $CMAKE_VERSION"
}

# Check if git is available
check_git() {
    if ! command -v git &> /dev/null; then
        print_error "Git is not installed. Please install git first."
        exit 1
    fi
}

# Check if make is available
check_make() {
    if ! command -v make &> /dev/null; then
        print_error "Make is not installed. Please install build-essential or development tools."
        print_info "Ubuntu/Debian: sudo apt-get install build-essential"
        print_info "CentOS/RHEL: sudo yum groupinstall 'Development Tools'"
        print_info "Arch: sudo pacman -S base-devel"
        exit 1
    fi
}

# Function to install system dependencies
install_system_deps() {
    print_info "Installing system dependencies..."
    
    # Detect OS
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS=$NAME
        print_info "Detected OS: $OS"
        
        case $OS in
            "Ubuntu"*)
                sudo apt-get update
                sudo apt-get install -y \
                    build-essential \
                    cmake \
                    git \
                    wget \
                    curl \
                    libasound2-dev \
                    libpulse-dev \
                    pkg-config
                ;;
            "CentOS"*|"Red Hat"*)
                sudo yum groupinstall -y "Development Tools"
                sudo yum install -y \
                    cmake \
                    git \
                    wget \
                    curl \
                    alsa-lib-devel \
                    pulseaudio-libs-devel \
                    pkgconfig
                ;;
            "Arch Linux")
                sudo pacman -S --needed \
                    base-devel \
                    cmake \
                    git \
                    wget \
                    curl \
                    alsa-lib \
                    libpulse \
                    pkgconf
                ;;
            *)
                print_warning "Unknown OS. Please install dependencies manually:"
                print_warning "- build-essential/development tools"
                print_warning "- cmake"
                print_warning "- git"
                print_warning "- audio libraries (alsa, pulseaudio)"
                ;;
        esac
    fi
}

# Main installation function
install_sherpa_onnx() {
    print_info "Starting sherpa-onnx installation..."
    
    # Set installation directory to user's .local directory
    USER_INSTALL_DIR="$HOME/.local"
    INSTALL_DIR="/tmp/sherpa-onnx-build"
    CURRENT_DIR=$(pwd)
    
    print_info "Building sherpa-onnx in: $INSTALL_DIR"
    print_info "Will install to user directory: $USER_INSTALL_DIR"
    
    # Ensure user local directories exist
    mkdir -p "$USER_INSTALL_DIR/lib"
    mkdir -p "$USER_INSTALL_DIR/include"
    mkdir -p "$USER_INSTALL_DIR/bin"
    
    # Clean up any existing build directory
    if [[ -d "$INSTALL_DIR" ]]; then
        print_warning "Removing existing build directory..."
        rm -rf "$INSTALL_DIR"
    fi
    
    # Clone sherpa-onnx repository
    print_info "Cloning sherpa-onnx repository..."
    git clone https://github.com/k2-fsa/sherpa-onnx "$INSTALL_DIR"
    cd "$INSTALL_DIR"
    
    # Create build directory
    print_info "Creating build directory..."
    mkdir -p build
    cd build
    
    # Configure with CMake
    print_info "Configuring with CMake..."
    
    # Load unified configuration
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    source "$SCRIPT_DIR/sherpa_config.sh"
    
    # Generate CMake options using unified configuration
    SHARED_LIBS_OPTION=$(get_shared_libs_option)
    CMAKE_OPTIONS=$(generate_cmake_options "$USER_INSTALL_DIR" "$SHARED_LIBS_OPTION")
    
    if [[ "$SHARED_LIBS_OPTION" == "ON" ]]; then
        print_info "Building shared libraries (GCC <= 10 detected) with minimal dependencies"
    else
        print_info "Building static libraries (default) with minimal dependencies"
    fi
    
    show_config_info
    print_info "Running CMake with options: $CMAKE_OPTIONS"
    cmake $CMAKE_OPTIONS ..
    
    # Build
    print_info "Building sherpa-onnx..."
    NPROC=$(nproc)
    print_info "Using $NPROC parallel jobs"
    make -j$NPROC
    
    # Install to user directory (no sudo needed)
    print_info "Installing sherpa-onnx to user directory..."
    make install
    
    # Update environment for current session
    print_info "Setting up environment..."
    export LD_LIBRARY_PATH="$USER_INSTALL_DIR/lib:$LD_LIBRARY_PATH"
    export PKG_CONFIG_PATH="$USER_INSTALL_DIR/lib/pkgconfig:$PKG_CONFIG_PATH"
    
    print_info "Installation completed successfully!"
    
    # Return to original directory
    cd "$CURRENT_DIR"
    
    # Verify installation - check user directory paths
    print_info "Verifying installation..."
    
    # Check libraries in user directory
    FOUND_LIBS=0
    for lib in sherpa-onnx-core sherpa-onnx-cxx-api sherpa-onnx-c-api; do
        if [[ -f "$USER_INSTALL_DIR/lib/lib${lib}.a" || -f "$USER_INSTALL_DIR/lib/lib${lib}.so" ]]; then
            print_info "✓ Found library: $lib"
            ((FOUND_LIBS++))
        else
            print_warning "! Library not found: $lib"
        fi
    done
    
    # Check headers in user directory
    if [[ -f "$USER_INSTALL_DIR/include/sherpa-onnx/c-api/c-api.h" ]]; then
        print_info "✓ sherpa-onnx headers found in user directory"
    else
        print_warning "! sherpa-onnx headers not found in $USER_INSTALL_DIR/include"
    fi
    
    if [[ $FOUND_LIBS -ge 2 ]]; then
        print_info "✓ Installation verification successful - found core libraries"
    else
        print_warning "! Installation verification failed - some libraries missing"
    fi
    
    # Print environment setup instructions
    print_info ""
    print_info "Environment Setup Instructions:"
    print_info "==============================="
    print_info "To use sherpa-onnx in future sessions, add the following to your ~/.bashrc or ~/.zshrc:"
    print_info "export LD_LIBRARY_PATH=\"\$HOME/.local/lib:\$LD_LIBRARY_PATH\""
    print_info "export PKG_CONFIG_PATH=\"\$HOME/.local/lib/pkgconfig:\$PKG_CONFIG_PATH\""
}

# Cleanup function
cleanup() {
    print_info "Cleaning up build files..."
    if [[ -n "$INSTALL_DIR" && -d "$INSTALL_DIR" ]]; then
        rm -rf "$INSTALL_DIR"
        print_info "Temporary build directory cleaned: $INSTALL_DIR"
    fi
}

# Main execution
main() {
    print_info "sherpa-onnx Installation Script"
    print_info "==============================="
    
    # Check prerequisites
    check_root
    check_git
    check_cmake
    check_make
    check_gcc_version
    
    # Ask if user wants to install system dependencies
    read -p "Install system dependencies? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_system_deps
    fi
    
    # Install sherpa-onnx
    install_sherpa_onnx
    
    # Ask if user wants to cleanup
    read -p "Clean up build files? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        cleanup
    fi
    
    print_info "Installation process completed!"
    print_info "You can now build your project with: ./build.sh"
}

# Run main function
main "$@"
