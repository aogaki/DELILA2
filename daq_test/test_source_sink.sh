#!/bin/bash

# Test script for DELILA2 source/sink components

# Set up ROOT environment
source /opt/ROOT/bin/thisroot.sh

echo "=== DELILA2 Source/Sink Test ==="
echo "This test will run for 10 seconds"
echo

# Start source in background
echo "Starting data source..."
./daq_test/build/bin/delila_source daq_test/PSD2.conf "tcp://*:5555" > source.log 2>&1 &
SOURCE_PID=$!

# Wait for source to initialize
sleep 3

# Start sink (without monitor to avoid ROOT display issues in test)
echo "Starting data sink..."
./daq_test/build/bin/delila_sink --address tcp://localhost:5555 --no-monitor --output-prefix test_run > sink.log 2>&1 &
SINK_PID=$!

# Let them run for 10 seconds
echo "Running data acquisition for 10 seconds..."
sleep 10

# Stop processes
echo "Stopping processes..."
kill $SINK_PID 2>/dev/null
kill $SOURCE_PID 2>/dev/null

# Wait for processes to finish
wait $SINK_PID 2>/dev/null
wait $SOURCE_PID 2>/dev/null

echo
echo "=== Test Results ==="

# Show last few lines of source log
echo "Source statistics:"
tail -n 20 source.log | grep -E "(Events:|Rate:|Final Statistics|Total events:|Average)"

echo
echo "Sink statistics:"
tail -n 20 sink.log | grep -E "(Events:|Rate:|Final Statistics|Total events:|Average)"

# Check if data files were created
echo
echo "Data files created:"
ls -la test_run*.dat 2>/dev/null || echo "No data files created (no events detected)"

echo
echo "Test complete!"