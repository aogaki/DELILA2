#!/bin/bash

# DELILA2 Network Library - Example Build Script
# This script builds the toy programs for testing the network library

set -e  # Exit on any error

echo "DELILA2 Network Library - Building Examples"
echo "==========================================="

# Check if we're in the right directory
if [ ! -f "sender.cpp" ] || [ ! -f "receiver.cpp" ]; then
    echo "Error: Please run this script from the examples directory"
    echo "Current directory: $(pwd)"
    exit 1
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring build..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the examples
echo "Building examples..."
make -j$(nproc 2>/dev/null || echo 4)

# Check if build was successful
if [ -x "./sender" ] && [ -x "./receiver" ] && [ -x "./performance_test" ]; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    echo "Built executables:"
    echo "  ./sender              - Data publisher/sender"
    echo "  ./receiver            - Data subscriber/receiver"  
    echo "  ./performance_test    - Comprehensive benchmarks"
    echo ""
    echo "Quick Start:"
    echo "============"
    echo ""
    echo "1. Run performance benchmarks:"
    echo "   ./performance_test"
    echo ""
    echo "2. Test network communication:"
    echo "   Terminal 1: ./receiver tcp://localhost:5555"
    echo "   Terminal 2: ./sender tcp://*:5555 10 50 100"
    echo ""
    echo "3. High-throughput test:"
    echo "   Terminal 1: ./receiver tcp://localhost:5555 60 received_data.csv"
    echo "   Terminal 2: ./sender tcp://*:5555 100 200 50"
    echo ""
    echo "Arguments:"
    echo "  sender <address> <batch_size> <num_batches> <delay_ms>"
    echo "  receiver <address> <timeout_s> [output_file]"
    echo ""
    echo "Example addresses:"
    echo "  tcp://*:5555      (bind to all interfaces, port 5555)"
    echo "  tcp://localhost:5555  (connect to localhost, port 5555)"
    echo "  tcp://192.168.1.100:5555  (connect to specific IP)"
    echo ""
else
    echo "❌ Build failed!"
    echo "Please check the error messages above."
    exit 1
fi