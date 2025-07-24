#!/bin/bash

# Kill all running test processes

echo "Stopping all DELILA network test processes..."

# Kill all test processes
pkill -f data_sender || true
pkill -f data_hub || true
pkill -f data_receiver || true

# Wait a moment for graceful shutdown
sleep 2

# Force kill if still running
pkill -9 -f data_sender || true
pkill -9 -f data_hub || true
pkill -9 -f data_receiver || true

echo "All processes stopped."