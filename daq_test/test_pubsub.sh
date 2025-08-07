#!/bin/bash

# Test script for DELILA2 one publisher, two subscribers pattern
# Tests: delila_source (publisher) -> delila_monitor + delila_recorder (subscribers)

# Set up ROOT environment
source /opt/ROOT/bin/thisroot.sh

echo "=== DELILA2 Publisher/Subscriber Test ==="
echo "Testing: 1 Publisher (source) -> 2 Subscribers (monitor + recorder)"
echo "This test will run for 15 seconds"
echo

# Clean up any previous test files
rm -f test_pubsub_*.dat 2>/dev/null
rm -f source_pubsub.log monitor_pubsub.log recorder_pubsub.log 2>/dev/null

# Start source (publisher) in background
echo "Starting data source (publisher)..."
./daq_test/build/bin/delila_source daq_test/PSD2.conf "tcp://*:5555" > source_pubsub.log 2>&1 &
SOURCE_PID=$!

# Wait for source to initialize
sleep 3

# Start monitor (subscriber 1) in background - no web server to avoid display issues
echo "Starting data monitor (subscriber 1)..."
./daq_test/build/bin/delila_monitor --address tcp://localhost:5555 --no-web > monitor_pubsub.log 2>&1 &
MONITOR_PID=$!

# Start recorder (subscriber 2) in background
echo "Starting data recorder (subscriber 2)..."
./daq_test/build/bin/delila_recorder --address tcp://localhost:5555 --output-prefix test_pubsub --threads 2 > recorder_pubsub.log 2>&1 &
RECORDER_PID=$!

# Let them run for 15 seconds
echo "Running data acquisition for 15 seconds..."
echo "  Publisher: Broadcasting events on tcp://*:5555"
echo "  Monitor: Receiving and analyzing events"
echo "  Recorder: Receiving and saving events to disk"
echo

# Show real-time stats
for i in {1..15}; do
    sleep 1
    printf "\rTime: %2d/15s" $i
done
echo

# Stop processes
echo
echo "Stopping processes..."
kill $RECORDER_PID 2>/dev/null
kill $MONITOR_PID 2>/dev/null
kill $SOURCE_PID 2>/dev/null

# Wait for processes to finish
wait $RECORDER_PID 2>/dev/null
wait $MONITOR_PID 2>/dev/null
wait $SOURCE_PID 2>/dev/null

echo
echo "=== Test Results ==="
echo

# Show source statistics
echo "SOURCE (Publisher) Statistics:"
echo "------------------------------"
tail -n 20 source_pubsub.log | grep -E "(Events:|Rate:|Final Statistics|Total events:|Average|Success Rate)" | sed 's/^/  /'

echo
echo "MONITOR (Subscriber 1) Statistics:"
echo "-----------------------------------"
tail -n 20 monitor_pubsub.log | grep -E "(\[MONITOR\]|Final Statistics|Total events:|Average|Missed)" | sed 's/^/  /'

echo
echo "RECORDER (Subscriber 2) Statistics:"
echo "------------------------------------"
tail -n 20 recorder_pubsub.log | grep -E "(\[RECORDER\]|Final Statistics|Total events:|Average|Files created|Missed)" | sed 's/^/  /'

# Check if data files were created
echo
echo "Data Files Created:"
echo "-------------------"
ls -lh test_pubsub_*.dat 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}' || echo "  No data files created"

# Summary
echo
echo "=== Summary ==="
echo

# Extract key numbers
SOURCE_EVENTS=$(grep "Total events:" source_pubsub.log 2>/dev/null | tail -1 | awk '{print $3}')
MONITOR_EVENTS=$(grep "Total events:" monitor_pubsub.log 2>/dev/null | tail -1 | awk '{print $3}')
RECORDER_EVENTS=$(grep "Total events:" recorder_pubsub.log 2>/dev/null | tail -1 | awk '{print $3}')

if [ -n "$SOURCE_EVENTS" ] && [ -n "$MONITOR_EVENTS" ] && [ -n "$RECORDER_EVENTS" ]; then
    echo "Event Distribution:"
    echo "  Source published:  $SOURCE_EVENTS events"
    echo "  Monitor received:  $MONITOR_EVENTS events"
    echo "  Recorder received: $RECORDER_EVENTS events"
    
    # Check if both subscribers received same data
    if [ "$MONITOR_EVENTS" = "$RECORDER_EVENTS" ]; then
        echo
        echo "✓ SUCCESS: Both subscribers received the same number of events!"
    else
        echo
        echo "⚠ WARNING: Subscribers received different event counts"
        echo "  This may be due to timing differences in startup/shutdown"
    fi
else
    echo "Unable to extract event counts from logs"
fi

echo
echo "Test complete!"
echo "Log files: source_pubsub.log, monitor_pubsub.log, recorder_pubsub.log"