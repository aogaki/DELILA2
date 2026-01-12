#!/bin/bash
#
# stop_pipeline.sh - Stop DELILA2 pipeline gracefully
#
# Stops components in the correct order:
# 1. Emulators (upstream)
# 2. Merger
# 3. Writer and Monitor (downstream)
#

LOG_DIR="./logs"
PID_FILE="$LOG_DIR/pids.txt"

if [ ! -f "$PID_FILE" ]; then
    echo "ERROR: PID file not found: $PID_FILE"
    echo "Make sure the pipeline was started with start_pipeline.sh"
    exit 1
fi

# Load PIDs
source "$PID_FILE"

echo "=== Stopping DELILA2 Pipeline ==="
echo ""

# Stop upstream first
echo "[1/4] Stopping Emulators..."
if [ -n "$EMU0_PID" ] && kill -0 "$EMU0_PID" 2>/dev/null; then
    kill -SIGTERM "$EMU0_PID"
    echo "      Sent SIGTERM to Emulator 0 (PID $EMU0_PID)"
fi
if [ -n "$EMU1_PID" ] && kill -0 "$EMU1_PID" 2>/dev/null; then
    kill -SIGTERM "$EMU1_PID"
    echo "      Sent SIGTERM to Emulator 1 (PID $EMU1_PID)"
fi
sleep 2

echo "[2/4] Stopping Merger..."
if [ -n "$MERGER_PID" ] && kill -0 "$MERGER_PID" 2>/dev/null; then
    kill -SIGTERM "$MERGER_PID"
    echo "      Sent SIGTERM to Merger (PID $MERGER_PID)"
fi
sleep 2

echo "[3/4] Stopping Writer..."
if [ -n "$WRITER_PID" ] && kill -0 "$WRITER_PID" 2>/dev/null; then
    kill -SIGTERM "$WRITER_PID"
    echo "      Sent SIGTERM to Writer (PID $WRITER_PID)"
fi

echo "[4/4] Stopping Monitor..."
if [ -n "$MONITOR_PID" ] && kill -0 "$MONITOR_PID" 2>/dev/null; then
    kill -SIGTERM "$MONITOR_PID"
    echo "      Sent SIGTERM to Monitor (PID $MONITOR_PID)"
fi

sleep 2

# Verify all stopped
echo ""
echo "Verifying shutdown..."
STILL_RUNNING=""
for pid in $EMU0_PID $EMU1_PID $MERGER_PID $WRITER_PID $MONITOR_PID; do
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        STILL_RUNNING="$STILL_RUNNING $pid"
    fi
done

if [ -n "$STILL_RUNNING" ]; then
    echo "WARNING: Some processes still running:$STILL_RUNNING"
    echo "Use 'kill -9$STILL_RUNNING' to force stop"
else
    echo "All processes stopped successfully."
    rm -f "$PID_FILE"
fi

echo ""
echo "=== Pipeline Stopped ==="
