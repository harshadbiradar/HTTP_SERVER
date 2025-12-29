ulimit -n 65535

# # Test 1: Warm-up
# python3 test.py 50 10

# # Test 2: Moderate
# python3 test.py 200 20

# # Test 3: Push it
# python3 test.py 500 30

# # Test 4: Aggressive
# python3 test.py 1000 30

# python3 test.py 1500 30

# Test 5: Max laptop push (if stable)
# python3 test.py 2000 30


# python3 test.py 3000 30

# python3 test.py 4000 30

# python3 test.py 5000 30

# python3 test.py 6000 30

# python3 test.py 7000 30

# python3 test.py 8000 30

# python3 test.py 9000 30

# python3 test.py 10000 30



# Warm-up
# wrk -t12 -c1000 -d30s -s echo.lua http://localhost:8080

# # Push hard
# wrk -t12 -c2000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c3000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c4000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c5000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c6000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c7000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c8000 -d30s -s echo.lua http://localhost:8080
# wrk -t12 -c9000 -d30s -s echo.lua http://localhost:8080




#!/bin/bash

# Define Ranges: START STEP END
THREAD_RANGE=$(seq 5 1 12)          # 5, 6, 7... 12
EVENT_RANGE=$(seq 4096 2048 8192)   #  4096, 6144, 8192
QUEUE_RANGE="4096 8192 16384 32768" 
# BUFF_LENN="1024 2048 4096 8192"

for t in $THREAD_RANGE; do
    for e in $EVENT_RANGE; do
        for q in $QUEUE_RANGE; do
            # for b in $BUFF_LENN; do

                echo "Running: Threads=$t, MaxEvents=$e, Queue=$q"

                # Run the combined executable in background
                ../build/concurrent_server --threads $t --events $e --queue $q &
                CPP_PID=$!

                # Run Python test (passing the same params so Python knows what it's testing)
                # python3 test.py --threads $t --events $e --queue $q
                python3 test.py --port 8080 --connections 3000,5000,7000,9000 --duration 30
                
                # Stop the C++ app for the next iteration
                kill -9 $CPP_PID
                wait $CPP_PID 2>/dev/null
                sleep 1
            # done
        done
    done
done
