#!/bin/bash

# Benchmark Script for Your TCP Server
# Tests performance under various loads

PORT=8080
SERVER_HOST="localhost"

echo "═══════════════════════════════════════════════════════"
echo "  TCP Server Performance Benchmark"
echo "═══════════════════════════════════════════════════════"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if server is running
check_server() {
    nc -zv $SERVER_HOST $PORT &>/dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} Server is running on port $PORT"
        return 0
    else
        echo -e "${RED}✗${NC} Server is NOT running on port $PORT"
        echo "  Please start your server first!"
        exit 1
    fi
}

# Install dependencies if needed
install_deps() {
    echo ""
    echo "Checking dependencies..."
    
    # Check for wrk
    if ! command -v wrk &> /dev/null; then
        echo -e "${YELLOW}!${NC} wrk not found. Installing..."
        sudo apt-get update && sudo apt-get install -y wrk
    else
        echo -e "${GREEN}✓${NC} wrk is installed"
    fi
    
    # Check for tcpkali (better for raw TCP)
    if ! command -v tcpkali &> /dev/null; then
        echo -e "${YELLOW}!${NC} tcpkali not found. Install for better TCP benchmarking:"
        echo "  sudo apt-get install -y tcpkali"
    else
        echo -e "${GREEN}✓${NC} tcpkali is installed"
    fi
}

# Kernel tuning recommendations
check_kernel_limits() {
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  Kernel Configuration Check"
    echo "═══════════════════════════════════════════════════════"
    
    # File descriptor limit
    FD_LIMIT=$(ulimit -n)
    if [ $FD_LIMIT -lt 100000 ]; then
        echo -e "${YELLOW}!${NC} File descriptor limit: $FD_LIMIT (recommend 1048576)"
        echo "  Run: ulimit -n 1048576"
    else
        echo -e "${GREEN}✓${NC} File descriptor limit: $FD_LIMIT"
    fi
    
    # Socket buffer sizes
    RMEM=$(sysctl -n net.core.rmem_max)
    WMEM=$(sysctl -n net.core.wmem_max)
    echo "  Socket buffer sizes: rmem=$RMEM wmem=$WMEM"
    
    # Connection backlog
    SOMAXCONN=$(sysctl -n net.core.somaxconn)
    if [ $SOMAXCONN -lt 4096 ]; then
        echo -e "${YELLOW}!${NC} somaxconn: $SOMAXCONN (recommend 65535)"
    else
        echo -e "${GREEN}✓${NC} somaxconn: $SOMAXCONN"
    fi
}

# Benchmark with tcpkali (raw TCP)
bench_tcpkali() {
    if ! command -v tcpkali &> /dev/null; then
        echo "Skipping tcpkali benchmark (not installed)"
        return
    fi
    
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  tcpkali Benchmark (Raw TCP)"
    echo "═══════════════════════════════════════════════════════"
    
    # Light load
    echo ""
    echo "Test 1: Light Load (100 connections, 10 seconds)"
    tcpkali -c100 -T10s -m "test\n" --message-rate=10000 $SERVER_HOST:$PORT
    
    # Medium load
    echo ""
    echo "Test 2: Medium Load (500 connections, 30 seconds)"
    tcpkali -c500 -T30s -m "test\n" --message-rate=50000 $SERVER_HOST:$PORT
    
    # Heavy load
    echo ""
    echo "Test 3: Heavy Load (2000 connections, 30 seconds)"
    tcpkali -c2000 -T30s -m "test\n" --message-rate=200000 $SERVER_HOST:$PORT
}

# Simple custom benchmark using netcat
bench_netcat() {
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  Simple Latency Test (nc)"
    echo "═══════════════════════════════════════════════════════"
    
    echo "Sending 10 requests..."
    for i in {1..10}; do
        start=$(date +%s%N)
        echo "test" | nc $SERVER_HOST $PORT > /dev/null
        end=$(date +%s%N)
        latency=$(( ($end - $start) / 1000000 ))
        echo "  Request $i: ${latency}ms"
    done
}

# Python benchmark script (more detailed)
bench_python() {
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  Custom Python Benchmark"
    echo "═══════════════════════════════════════════════════════"
    
    cat > /tmp/bench_server.py << 'PYTHON_EOF'
import socket
import time
import statistics
from concurrent.futures import ThreadPoolExecutor

HOST = 'localhost'
PORT = 8080
NUM_REQUESTS = 10000
NUM_THREADS = 10

def send_requests(n):
    latencies = []
    for _ in range(n):
        try:
            start = time.perf_counter()
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((HOST, PORT))
            s.sendall(b'test\n')
            response = s.recv(1024)
            s.close()
            end = time.perf_counter()
            latencies.append((end - start) * 1000)  # ms
        except Exception as e:
            print(f"Error: {e}")
    return latencies

print(f"Running {NUM_REQUESTS} requests across {NUM_THREADS} threads...")
start_time = time.time()

with ThreadPoolExecutor(max_workers=NUM_THREADS) as executor:
    futures = [executor.submit(send_requests, NUM_REQUESTS // NUM_THREADS) 
               for _ in range(NUM_THREADS)]
    all_latencies = []
    for future in futures:
        all_latencies.extend(future.result())

end_time = time.time()
duration = end_time - start_time

print(f"\n✓ Completed {len(all_latencies)} requests in {duration:.2f}s")
print(f"  Throughput: {len(all_latencies) / duration:.0f} req/sec")
print(f"  Avg latency: {statistics.mean(all_latencies):.2f}ms")
print(f"  p50 latency: {statistics.median(all_latencies):.2f}ms")
print(f"  p99 latency: {sorted(all_latencies)[int(len(all_latencies)*0.99)]:.2f}ms")
PYTHON_EOF

    python3 /tmp/bench_server.py
    rm /tmp/bench_server.py
}

# Main execution
main() {
    check_server
    install_deps
    check_kernel_limits
    
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  Starting Benchmarks"
    echo "═══════════════════════════════════════════════════════"
    
    bench_netcat
    bench_python
    bench_tcpkali
    
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  Benchmark Complete!"
    echo "═══════════════════════════════════════════════════════"
    echo ""
    echo "Tips for better performance:"
    echo "  1. Increase ulimit -n 1048576"
    echo "  2. Tune kernel: sysctl -w net.core.somaxconn=65535"
    echo "  3. Use more worker threads (--threads)"
    echo "  4. Enable CPU affinity"
    echo "  5. Profile with: perf record -g ./your_server"
}

# Run it
main
