ulimit -n 65535

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