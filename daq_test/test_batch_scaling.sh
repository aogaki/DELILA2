#!/bin/bash

# Batch size scaling test to find saturation point
# Fixed event size, increasing batch sizes

echo "=== DELILA2 Batch Size Scaling Test ==="
echo "Finding throughput saturation point"
echo "Fixed: Event Size = 1024 bytes, Speed = unlimited"
echo

# Clean up any previous logs
rm -f batch_test_*.log 2>/dev/null

# Function to run a single batch size test
run_batch_test() {
    local batch_size="$1"
    local duration="$2"
    local pool_size="$3"
    
    echo "Testing batch size: $batch_size events ($((batch_size * 1024 / 1024)) MB per batch)"
    
    # Calculate appropriate pool size (at least 2x batch count expected)
    local recommended_pool=$((batch_size / 1000 * 20))
    if [ $recommended_pool -gt $pool_size ]; then
        pool_size=$recommended_pool
    fi
    
    # Start emulator
    ./daq_test/build/bin/delila_emulator \
        --event-size 1024 \
        --batch-size "$batch_size" \
        --unlimited \
        --pool-size "$pool_size" \
        > "batch_test_${batch_size}.log" 2>&1 &
    local emulator_pid=$!
    
    # Let it run for specified duration
    sleep "$duration"
    
    # Stop emulator
    kill $emulator_pid 2>/dev/null
    wait $emulator_pid 2>/dev/null
    
    # Extract final statistics
    local last_line=$(tail -10 "batch_test_${batch_size}.log" | grep "Rate:" | tail -1)
    if [ -n "$last_line" ]; then
        local event_rate=$(echo "$last_line" | sed -n 's/.*Rate: \([0-9.e+]*\) evt\/s.*/\1/p')
        local data_rate=$(echo "$last_line" | sed -n 's/.*Data: \([0-9.]*\) MB\/s.*/\1/p')
        local pool_usage=$(echo "$last_line" | sed -n 's/.*Pool: \([0-9]*\)\/\([0-9]*\).*/\1\/\2/p')
        
        printf "  %-10s | %-12s | %-10s | %-15s | %s\n" \
            "$batch_size" "$event_rate" "$data_rate" "$(echo "$event_rate" | awk '{print $1 * 1024 / 1024 / 1024}' 2>/dev/null | head -c 6)GB/s" "$pool_usage"
    else
        echo "  Failed to extract statistics"
    fi
}

# Test header
printf "%-10s | %-12s | %-10s | %-15s | %s\n" "Batch Size" "Event Rate" "Data Rate" "Throughput" "Pool Usage"
printf "%-10s | %-12s | %-10s | %-15s | %s\n" "----------" "------------" "----------" "---------------" "----------"

# Progressive batch size tests
# Start small and scale up exponentially

echo "Phase 1: Small to Medium Batches"
run_batch_test 1000 8 5000
run_batch_test 2500 8 5000
run_batch_test 5000 8 5000
run_batch_test 10000 8 10000

echo
echo "Phase 2: Large Batches"
run_batch_test 25000 10 15000
run_batch_test 50000 10 20000
run_batch_test 75000 10 25000
run_batch_test 100000 10 30000

echo
echo "Phase 3: Extreme Batches"
run_batch_test 150000 12 40000
run_batch_test 200000 12 50000
run_batch_test 250000 12 60000
run_batch_test 300000 15 70000

echo
echo "Phase 4: Maximum Batches (if system can handle)"
echo "Warning: These may use significant memory"
run_batch_test 400000 15 80000
run_batch_test 500000 15 100000

echo
echo "=== Analysis ==="
echo "Looking for saturation patterns..."

# Analyze the results
echo
echo "Event Rate Progression:"
for log in batch_test_*.log; do
    if [ -f "$log" ]; then
        batch_size=$(basename "$log" .log | sed 's/batch_test_//')
        last_rate=$(tail -10 "$log" | grep "Rate:" | tail -1 | sed -n 's/.*Rate: \([0-9.e+]*\) evt\/s.*/\1/p')
        if [ -n "$last_rate" ]; then
            printf "  Batch %6s: %s events/s\n" "$batch_size" "$last_rate"
        fi
    fi
done

echo
echo "Data Rate Progression:"
for log in batch_test_*.log; do
    if [ -f "$log" ]; then
        batch_size=$(basename "$log" .log | sed 's/batch_test_//')
        last_data=$(tail -10 "$log" | grep "Data:" | tail -1 | sed -n 's/.*Data: \([0-9.]*\) MB\/s.*/\1/p')
        if [ -n "$last_data" ]; then
            printf "  Batch %6s: %s MB/s\n" "$batch_size" "$last_data"
        fi
    fi
done

echo
echo "=== Test Complete ==="
echo "Check batch_test_*.log files for detailed performance data"
echo "Look for plateaus in event rate and data rate to identify saturation points"