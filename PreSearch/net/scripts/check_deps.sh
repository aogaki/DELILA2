#!/bin/bash

# Check dependencies for DELILA network performance test

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Checking dependencies for DELILA Network Performance Test...${NC}"

# Check for required tools
echo -e "${YELLOW}Checking build tools...${NC}"

check_command() {
    if command -v "$1" >/dev/null 2>&1; then
        echo -e "${GREEN}✓ $1 found${NC}"
    else
        echo -e "${RED}✗ $1 not found${NC}"
        return 1
    fi
}

# Essential build tools
check_command cmake
check_command make
check_command g++
check_command pkg-config

# Check for protobuf
echo -e "${YELLOW}Checking protobuf...${NC}"
if command -v protoc >/dev/null 2>&1; then
    echo -e "${GREEN}✓ protoc found ($(protoc --version))${NC}"
else
    echo -e "${RED}✗ protoc not found${NC}"
    MISSING_DEPS=true
fi

# Check for gRPC
echo -e "${YELLOW}Checking gRPC...${NC}"
if command -v grpc_cpp_plugin >/dev/null 2>&1; then
    echo -e "${GREEN}✓ grpc_cpp_plugin found${NC}"
else
    echo -e "${RED}✗ grpc_cpp_plugin not found${NC}"
    MISSING_DEPS=true
fi

# Check for development libraries
echo -e "${YELLOW}Checking development libraries...${NC}"

check_pkg_config() {
    if pkg-config --exists "$1" 2>/dev/null; then
        echo -e "${GREEN}✓ $1 found ($(pkg-config --modversion $1))${NC}"
    else
        echo -e "${RED}✗ $1 not found${NC}"
        return 1
    fi
}

# Check libraries
check_pkg_config protobuf || MISSING_DEPS=true
check_pkg_config grpc++ || MISSING_DEPS=true
check_pkg_config libzmq || MISSING_DEPS=true

# Check for optional tools
echo -e "${YELLOW}Checking optional tools...${NC}"
check_command git || echo -e "${YELLOW}  (git not required but recommended)${NC}"
check_command python3 || echo -e "${YELLOW}  (python3 not required but useful for analysis)${NC}"

echo

if [ "$MISSING_DEPS" = true ]; then
    echo -e "${RED}Some dependencies are missing. Install them with:${NC}"
    echo
    echo -e "${YELLOW}Ubuntu/Debian:${NC}"
    echo "sudo apt-get update"
    echo "sudo apt-get install -y \\"
    echo "    cmake \\"
    echo "    build-essential \\"
    echo "    pkg-config \\"
    echo "    protobuf-compiler \\"
    echo "    libprotobuf-dev \\"
    echo "    libgrpc++-dev \\"
    echo "    libzmq3-dev \\"
    echo "    git"
    echo
    echo -e "${YELLOW}Then install cppzmq (header-only library):${NC}"
    echo "git clone https://github.com/zeromq/cppzmq.git"
    echo "cd cppzmq && mkdir build && cd build"
    echo "cmake .. && sudo make install"
    echo
    exit 1
else
    echo -e "${GREEN}All dependencies are installed!${NC}"
    echo -e "${GREEN}You can now run: ./build.sh${NC}"
    exit 0
fi