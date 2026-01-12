#!/bin/bash
#
# start_pipeline.sh - Start a complete DELILA2 pipeline
#
# This script demonstrates how to start all components in the correct order.
# Each component runs in the background with output redirected to log files.
#
# Usage:
#   ./start_pipeline.sh [data_dir]
#
# Default data directory: ./data
#

set -e

# Configuration
DATA_DIR="${1:-./data}"
LOG_DIR="./logs"

# Ports
EMU0_PORT=5555
EMU1_PORT=5556
MERGER_PORT=5560
HTTP_PORT=8080

# Create directories
mkdir -p "$DATA_DIR"
mkdir -p "$LOG_DIR"

echo "=== DELILA2 Pipeline Startup ==="
echo "Data directory: $DATA_DIR"
echo "Log directory:  $LOG_DIR"
echo ""

# Find executables (in build/examples or current directory)
if [ -f "./build/examples/delila_writer" ]; then
    BIN_DIR="./build/examples"
elif [ -f "./delila_writer" ]; then
    BIN_DIR="."
else
    echo "ERROR: Cannot find DELILA2 executables."
    echo "Please build the project first or run from the correct directory."
    exit 1
fi

echo "Using executables from: $BIN_DIR"
echo ""

# Start FileWriter (downstream must start first)
echo "[1/4] Starting FileWriter..."
"$BIN_DIR/delila_writer" \
    -i "tcp://localhost:$MERGER_PORT" \
    -d "$DATA_DIR" \
    -p "run_" \
    > "$LOG_DIR/writer.log" 2>&1 &
WRITER_PID=$!
echo "      PID: $WRITER_PID, Log: $LOG_DIR/writer.log"
sleep 1

# Start MonitorROOT (if available)
if [ -f "$BIN_DIR/delila_monitor" ]; then
    echo "[2/4] Starting MonitorROOT..."
    "$BIN_DIR/delila_monitor" \
        -i "tcp://localhost:$MERGER_PORT" \
        -p "$HTTP_PORT" \
        --2d \
        > "$LOG_DIR/monitor.log" 2>&1 &
    MONITOR_PID=$!
    echo "      PID: $MONITOR_PID, Log: $LOG_DIR/monitor.log"
    echo "      Web UI: http://localhost:$HTTP_PORT"
    sleep 2
else
    echo "[2/4] MonitorROOT not available (ROOT not installed)"
    MONITOR_PID=""
fi

# Start SimpleMerger
echo "[3/4] Starting SimpleMerger..."
"$BIN_DIR/delila_merger" \
    -i "tcp://localhost:$EMU0_PORT" \
    -i "tcp://localhost:$EMU1_PORT" \
    -o "tcp://*:$MERGER_PORT" \
    > "$LOG_DIR/merger.log" 2>&1 &
MERGER_PID=$!
echo "      PID: $MERGER_PID, Log: $LOG_DIR/merger.log"
sleep 1

# Start Emulators (upstream starts last)
echo "[4/4] Starting Emulators..."
"$BIN_DIR/delila_emulator" \
    -o "tcp://*:$EMU0_PORT" \
    -m 0 \
    -r 1000 \
    > "$LOG_DIR/emulator0.log" 2>&1 &
EMU0_PID=$!
echo "      Emulator 0: PID $EMU0_PID"

"$BIN_DIR/delila_emulator" \
    -o "tcp://*:$EMU1_PORT" \
    -m 1 \
    -r 1000 \
    > "$LOG_DIR/emulator1.log" 2>&1 &
EMU1_PID=$!
echo "      Emulator 1: PID $EMU1_PID"

echo ""
echo "=== Pipeline Started ==="
echo ""
echo "PIDs:"
echo "  Emulator 0: $EMU0_PID"
echo "  Emulator 1: $EMU1_PID"
echo "  Merger:     $MERGER_PID"
echo "  Writer:     $WRITER_PID"
if [ -n "$MONITOR_PID" ]; then
    echo "  Monitor:    $MONITOR_PID"
fi
echo ""
echo "To stop the pipeline:"
echo "  kill $EMU0_PID $EMU1_PID $MERGER_PID $WRITER_PID $MONITOR_PID"
echo ""
echo "Or use: ./stop_pipeline.sh"
echo ""

# Save PIDs for stop script
cat > "$LOG_DIR/pids.txt" << PIDS
EMU0_PID=$EMU0_PID
EMU1_PID=$EMU1_PID
MERGER_PID=$MERGER_PID
WRITER_PID=$WRITER_PID
MONITOR_PID=$MONITOR_PID
PIDS

echo "Monitoring logs (Ctrl+C to exit, pipeline continues running):"
echo "============================================================"
tail -f "$LOG_DIR"/*.log
