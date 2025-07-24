#!/bin/bash

# Run test script for DELILA network performance test

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
CONFIG_FILE="${1:-config/config.json}"
PROTOCOL="${2:-grpc}"

echo -e "${GREEN}Running DELILA Network Performance Test${NC}"
echo -e "${YELLOW}Protocol: $PROTOCOL${NC}"
echo -e "${YELLOW}Config: $CONFIG_FILE${NC}"

# Check if binaries exist
if [ ! -f "build/data_sender" ] || [ ! -f "build/data_hub" ] || [ ! -f "build/data_receiver" ]; then
    echo -e "${RED}Error: Binaries not found. Run build.sh first.${NC}"
    exit 1
fi

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo -e "${RED}Error: Config file not found: $CONFIG_FILE${NC}"
    exit 1
fi

# Create required directories
mkdir -p logs results

# Update config for protocol
if [ "$PROTOCOL" = "zmq" ] || [ "$PROTOCOL" = "zeromq" ]; then
    sed -i 's/"protocol": "grpc"/"protocol": "zeromq"/g' "$CONFIG_FILE"
else
    sed -i 's/"protocol": "zeromq"/"protocol": "grpc"/g' "$CONFIG_FILE"
fi

echo -e "${YELLOW}Starting test components...${NC}"

# Function to cleanup processes
cleanup() {
    echo -e "${YELLOW}Cleaning up processes...${NC}"
    pkill -f data_sender || true
    pkill -f data_hub || true
    pkill -f data_receiver || true
    wait
    echo -e "${GREEN}Cleanup complete${NC}"
}

# Set trap for cleanup
trap cleanup EXIT

# Start data hub
echo -e "${YELLOW}Starting data hub...${NC}"
./build/data_hub "$CONFIG_FILE" &
HUB_PID=$!

# Give hub time to start
sleep 2

# Start data receivers
echo -e "${YELLOW}Starting data receivers...${NC}"
./build/data_receiver "$CONFIG_FILE" 1 &
RECEIVER1_PID=$!

./build/data_receiver "$CONFIG_FILE" 2 &
RECEIVER2_PID=$!

# Give receivers time to start
sleep 2

# Start data senders
echo -e "${YELLOW}Starting data senders...${NC}"
./build/data_sender "$CONFIG_FILE" 1 &
SENDER1_PID=$!

./build/data_sender "$CONFIG_FILE" 2 &
SENDER2_PID=$!

echo -e "${GREEN}All components started. Test is running...${NC}"
echo -e "${YELLOW}Press Ctrl+C to stop the test${NC}"

# Wait for senders to complete (they will exit after test duration)
wait $SENDER1_PID
wait $SENDER2_PID

echo -e "${GREEN}Test completed successfully!${NC}"
echo -e "${YELLOW}Check results/ directory for output files${NC}"
echo -e "${YELLOW}Check logs/ directory for log files${NC}"

# Stop remaining processes
kill $HUB_PID $RECEIVER1_PID $RECEIVER2_PID 2>/dev/null || true
wait