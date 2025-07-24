#!/bin/bash

# Build script for DELILA network performance test

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building DELILA Network Performance Test...${NC}"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

# Check if executables were built
if [ -f "data_sender" ] && [ -f "data_hub" ] && [ -f "data_receiver" ] && [ -f "test_runner" ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo "Executables created:"
    ls -la data_sender data_hub data_receiver test_runner
else
    echo -e "${RED}Build failed - not all executables were created${NC}"
    exit 1
fi

echo -e "${GREEN}Build complete. Run executables from build/ directory.${NC}"