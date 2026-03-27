#!/bin/bash

ulimit -n 1048576

# Create results directory
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="stress_test_results_${TIMESTAMP}"
mkdir -p $RESULTS_DIR

echo "Results will be saved to: $RESULTS_DIR"
echo "========================================"

# Define Ranges
THREAD_RANGE=$(seq 5 1 12)
EVENT_RANGE=$(seq 4096 2048 8192)
CONN_RANGE=$(seq 5000 5000 25000)

# Function to wait for server to be ready
wait_for_server() {
    local port=$1
    local max_wait=10
    local count=0
    
    echo "Waiting for server to start on port $port..."
    while ! nc -z localhost $port 2>/dev/null; do
        sleep 0.5
        count=$((count + 1))
        if [ $count -gt $max_wait ]; then
            echo "ERROR: Server failed to start within ${max_wait} seconds"
            return 1
        fi
    done
    echo "Server is ready!"
    return 0
}

# Test counter
TEST_NUM=0

for t in $THREAD_RANGE; do
    for e in $EVENT_RANGE; do
        for cn in $CONN_RANGE; do
            TEST_NUM=$((TEST_NUM + 1))
            
            echo ""
            echo "========================================"
            echo "Test #$TEST_NUM: Threads=$t, MaxEvents=$e, Connections=$cn"
            echo "========================================"
            
            # Create subdirectory for this test
            TEST_DIR="$RESULTS_DIR/test_${TEST_NUM}_t${t}_e${e}_c${cn}"
            mkdir -p $TEST_DIR
            
            # Start the server
            echo "Starting server..."
            ../build/concurrent_server --threads $t --maxperthrd $e > $TEST_DIR/server.log 2>&1 &
            CPP_PID=$!
            
            # Wait for server to be ready
            if ! wait_for_server 8080; then
                echo "Skipping test due to server startup failure"
                kill -9 $CPP_PID 2>/dev/null
                continue
            fi
            
            # Start CPU monitoring
            echo "Starting CPU monitoring..."
            mpstat -P ALL 2 > $TEST_DIR/cpu_cores.log &
            MPSTAT_PID=$!
            
            pidstat -t -p $CPP_PID 2 > $TEST_DIR/threads.log &
            PIDSTAT_PID=$!
            
            pidstat -w -p $CPP_PID 2 > $TEST_DIR/ctx_switches.log &
            PIDSTAT_CTX_PID=$!
            
            vmstat 2 > $TEST_DIR/vmstat.log &
            VMSTAT_PID=$!
            
            # Small delay to let monitoring stabilize
            sleep 2
            
            # Run wrk stress test
            echo "Running: wrk -t11 -c$cn -d20s --latency http://localhost:8080/"
            wrk -t11 -c$cn -d20s --latency http://localhost:8080/ > $TEST_DIR/wrk_results.txt 2>&1
            
            # Stop monitoring
            kill $MPSTAT_PID $PIDSTAT_PID $PIDSTAT_CTX_PID $VMSTAT_PID 2>/dev/null
            
            # Stop the server
            kill -9 $CPP_PID 2>/dev/null
            wait $CPP_PID 2>/dev/null
            
            # Extract key metrics from wrk output
            REQUESTS=$(grep "Requests/sec:" $TEST_DIR/wrk_results.txt | awk '{print $2}')
            LATENCY_AVG=$(grep "Latency" $TEST_DIR/wrk_results.txt | head -1 | awk '{print $2}')
            
            # Save test config and summary
            cat > $TEST_DIR/config.txt << EOF
Test Configuration:
------------------
Threads: $t
MaxEvents: $e
Connections: $cn

Results:
--------
Requests/sec: $REQUESTS
Avg Latency: $LATENCY_AVG

Full results in: $TEST_DIR/
EOF
            
            echo "Results: RPS=$REQUESTS, Latency=$LATENCY_AVG"
            echo "Saved to: $TEST_DIR/"
            
            # Wait a bit before next test
            sleep 3
        done
    done
done

# Generate overall summary
echo ""
echo "========================================"
echo "All tests complete! Generating summary..."
echo "========================================"

cat > $RESULTS_DIR/SUMMARY.txt << EOF
Stress Test Summary
===================
Date: $(date)
Total Tests: $TEST_NUM

Configuration Ranges:
- Threads: $(echo $THREAD_RANGE | awk '{print $1"-"$NF}')
- MaxEvents: $(echo $EVENT_RANGE | awk '{print $1"-"$NF}')
- Connections: $(echo $CONN_RANGE | awk '{print $1"-"$NF}')

Top 10 Configurations by RPS:
-----------------------------
EOF

# Find best configurations
for dir in $RESULTS_DIR/test_*/; do
    if [ -f "$dir/wrk_results.txt" ]; then
        RPS=$(grep "Requests/sec:" "$dir/wrk_results.txt" | awk '{print $2}')
        CONFIG=$(basename $dir)
        echo "$RPS $CONFIG"
    fi
done | sort -rn | head -10 >> $RESULTS_DIR/SUMMARY.txt

echo ""
cat $RESULTS_DIR/SUMMARY.txt
echo ""
echo "Detailed results saved in: $RESULTS_DIR/"