#!/bin/bash

# Build and run Recorder tests

echo "Building Recorder tests..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
make -j$(nproc)

# Run tests
echo "Running tests..."
./test_recorder

# Return exit code from tests
exit $?