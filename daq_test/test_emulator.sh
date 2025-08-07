#!/bin/bash

# Test script for DELILA2 emulator performance testing
# Tests different data sizes and throughput scenarios

echo "=== DELILA2 Emulator Performance Test ==="
echo

# Clean up any previous test files
rm -f emulator_*.log monitor_*.log recorder_*.log 2>/dev/null
rm -f test_emulator_*.dat 2>/dev/null

# Function to run a single test
run_test() {
    local test_name="$1"
    local event_size="$2"
    local batch_size="$3"
    local unlimited="$4"
    local duration="$5"
    
    echo "=== Test: $test_name ==="
    echo "  Event size: $event_size bytes"
    echo "  Batch size: $batch_size events"
    echo "  Speed: $([ "$unlimited" = "--unlimited" ] && echo "unlimited" || echo "limited")"
    echo "  Duration: $duration seconds"
    echo
    
    # Start emulator
    echo "Starting emulator..."
    if [ "$unlimited" = "--unlimited" ]; then
        ./daq_test/build/bin/delila_emulator --address "tcp://*:5555" \
            --event-size "$event_size" --batch-size "$batch_size" --unlimited \
            --pool-size 2000 > "emulator_${test_name}.log" 2>&1 &
    else
        ./daq_test/build/bin/delila_emulator --address "tcp://*:5555" \
            --event-size "$event_size" --batch-size "$batch_size" \
            --pool-size 2000 > "emulator_${test_name}.log" 2>&1 &
    fi
    EMULATOR_PID=$!
    
    # Wait for emulator to initialize
    sleep 2
    
    # Start monitor
    echo "Starting monitor..."
    ./daq_test/build/bin/delila_monitor --address tcp://localhost:5555 --no-web \
        > "monitor_${test_name}.log" 2>&1 &
    MONITOR_PID=$!
    
    # Start recorder
    echo "Starting recorder..."
    ./daq_test/build/bin/delila_recorder --address tcp://localhost:5555 \
        --output-prefix "test_emulator_${test_name}" --threads 4 \
        > "recorder_${test_name}.log" 2>&1 &
    RECORDER_PID=$!
    
    # Let them run
    echo "Running test for $duration seconds..."
    for i in $(seq 1 $duration); do
        printf "\r  Time: %2d/%ds" $i $duration
        sleep 1
    done
    echo
    
    # Stop processes
    echo "Stopping processes..."
    kill $RECORDER_PID 2>/dev/null
    kill $MONITOR_PID 2>/dev/null
    kill $EMULATOR_PID 2>/dev/null
    
    # Wait for processes to finish
    wait $RECORDER_PID 2>/dev/null
    wait $MONITOR_PID 2>/dev/null
    wait $EMULATOR_PID 2>/dev/null
    
    # Show results
    echo
    echo "Results for $test_name:"
    echo "------------------------"
    
    # Extract key metrics
    local emulator_rate=$(tail -10 "emulator_${test_name}.log" | grep "Rate:" | tail -1 | sed -n 's/.*Rate: \([0-9.]*\) evt\/s.*/\1/p')
    local emulator_data=$(tail -10 "emulator_${test_name}.log" | grep "Data:" | tail -1 | sed -n 's/.*Data: \([0-9.]*\) MB\/s.*/\1/p')
    local monitor_events=$(tail -5 "monitor_${test_name}.log" | grep "\[MONITOR\]" | tail -1 | sed -n 's/.*Events: \([0-9]*\).*/\1/p')
    local recorder_events=$(tail -5 "recorder_${test_name}.log" | grep "\[RECORDER\]" | tail -1 | sed -n 's/.*Events: \([0-9]*\).*/\1/p')
    local recorder_files=$(tail -5 "recorder_${test_name}.log" | grep "\[RECORDER\]" | tail -1 | sed -n 's/.*Files: \([0-9]*\).*/\1/p')
    
    if [ -n "$emulator_rate" ]; then
        echo "  Emulator: $emulator_rate events/s, $emulator_data MB/s"
    fi
    if [ -n "$monitor_events" ]; then
        echo "  Monitor received: $monitor_events events"
    fi
    if [ -n "$recorder_events" ] && [ -n "$recorder_files" ]; then
        echo "  Recorder saved: $recorder_events events in $recorder_files files"
    fi
    
    # Check pool usage
    local pool_usage=$(tail -10 "emulator_${test_name}.log" | grep "Pool:" | tail -1 | sed -n 's/.*Pool: \([0-9]*\/[0-9]*\).*/\1/p')
    if [ -n "$pool_usage" ]; then
        echo "  Pool usage: $pool_usage"
    fi
    
    echo
}

# Test 1: Small events, high rate
run_test "small_fast" 1024 50 "--unlimited" 10

# Test 2: Medium events, medium rate  
run_test "medium_normal" 8192 100 "" 10

# Test 3: Large events, unlimited speed
run_test "large_unlimited" 32768 25 "--unlimited" 10

# Test 4: Very large events to test memory limits
run_test "huge_events" 131072 10 "" 10

echo "=== Performance Test Summary ==="
echo
echo "Test files created:"
ls -la emulator_*.log monitor_*.log recorder_*.log test_emulator_*.dat 2>/dev/null | head -10

echo
echo "=== All Tests Complete ==="
echo
echo "Analysis:"
echo "- small_fast: Tests maximum event throughput with minimal data"
echo "- medium_normal: Tests realistic event sizes with standard batching"
echo "- large_unlimited: Tests large events at maximum speed"
echo "- huge_events: Tests memory and serialization limits"
echo
echo "Check the log files for detailed performance metrics!"