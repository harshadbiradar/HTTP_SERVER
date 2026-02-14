#!/bin/bash

# Server Performance Diagnostics Script
# Helps identify which bug is causing poor performance

set -e

PORT=8080
SERVER_BINARY="./server"  # Adjust to your binary name

echo "═══════════════════════════════════════════════════════"
echo "  Server Performance Diagnostics"
echo "═══════════════════════════════════════════════════════"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if server is running
check_server() {
    if pgrep -f "$SERVER_BINARY" > /dev/null; then
        echo -e "${GREEN}✓${NC} Server is running"
        SERVER_PID=$(pgrep -f "$SERVER_BINARY")
        echo "  PID: $SERVER_PID"
        return 0
    else
        echo -e "${YELLOW}!${NC} Server is NOT running"
        echo "  Please start your server first, then run this script again."
        return 1
    fi
}

# Bug #1: Check for write_sent reset bug
check_write_sent_bug() {
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "Checking for Bug #1: write_sent reset"
    echo "─────────────────────────────────────────────────────"
    
    if [ ! -f "../src/thread_pool.cpp" ]; then
        echo -e "${YELLOW}!${NC} Cannot find thread_pool.cpp in current directory"
        return
    fi
    
    if grep -q "write_sent = 0" thread_pool.cpp 2>/dev/null; then
        echo -e "${RED}✗ BUG FOUND!${NC}"
        echo ""
        echo "  Found 'write_sent = 0' in thread_pool.cpp:"
        grep -n "write_sent = 0" thread_pool.cpp
        echo ""
        echo "  This is CRITICAL! It resets write progress every event."
        echo "  Impact: Infinite write loops, 400ms+ latency"
        echo ""
        echo "  FIX: Remove this line from the event loop."
        echo "       It should only be reset AFTER completing all writes."
        return 1
    else
        echo -e "${GREEN}✓ OK${NC} - No write_sent reset bug found"
        return 0
    fi
}

# Bug #2: Check for vector-based connection pool
check_vector_bug() {
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "Checking for Bug #2: Vector-indexed connections"
    echo "─────────────────────────────────────────────────────"
    
    if [ ! -f "thread_pool.cpp" ]; then
        echo -e "${YELLOW}!${NC} Cannot find thread_pool.cpp"
        return
    fi
    
    if grep -q "std::vector<Connection.*conn_pool" thread_pool.cpp 2>/dev/null; then
        echo -e "${YELLOW}⚠ INEFFICIENCY DETECTED${NC}"
        echo ""
        echo "  Found vector-based connection pool:"
        grep -n "std::vector<Connection" thread_pool.cpp | head -5
        echo ""
        echo "  If indexed by fd (conn_pool[fd]), this wastes memory."
        echo "  Impact: Cache misses, memory waste, possible segfaults"
        echo ""
        echo "  RECOMMEND: Use std::unordered_map<int, Connection> instead"
        return 1
    else
        echo -e "${GREEN}✓ OK${NC} - Using unordered_map or efficient structure"
        return 0
    fi
}

# Check CPU usage
check_cpu_usage() {
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "Monitoring CPU Usage (5 seconds)"
    echo "─────────────────────────────────────────────────────"
    
    if ! check_server; then
        return
    fi
    
    echo "  Starting monitoring..."
    
    # Get initial CPU stats
    CPU_BEFORE=$(ps -p $SERVER_PID -o %cpu --no-headers 2>/dev/null || echo "0")
    
    # Send some load
    (for i in {1..1000}; do echo "test" | nc localhost $PORT > /dev/null 2>&1 || true; done) &
    LOAD_PID=$!
    
    sleep 3
    
    # Get CPU during load
    CPU_DURING=$(ps -p $SERVER_PID -o %cpu --no-headers 2>/dev/null || echo "0")
    
    kill $LOAD_PID 2>/dev/null || true
    wait $LOAD_PID 2>/dev/null || true
    
    echo "  CPU usage during load: ${CPU_DURING}%"
    
    if (( $(echo "$CPU_DURING > 300" | bc -l 2>/dev/null || echo 0) )); then
        echo -e "  ${RED}✗ HIGH CPU${NC} - Multiple threads at 100%"
        echo "    Likely cause: Busy loops (write_sent bug)"
    elif (( $(echo "$CPU_DURING > 100" | bc -l 2>/dev/null || echo 0) )); then
        echo -e "  ${GREEN}✓ NORMAL${NC} - Expected for multi-threaded server"
    else
        echo -e "  ${YELLOW}! LOW CPU${NC} - Server might be I/O bound or waiting"
    fi
}

# Check file descriptor usage
check_fd_limits() {
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "Checking System Limits"
    echo "─────────────────────────────────────────────────────"
    
    FD_LIMIT=$(ulimit -n)
    echo "  File descriptor limit: $FD_LIMIT"
    
    if [ $FD_LIMIT -lt 10000 ]; then
        echo -e "  ${YELLOW}! LOW${NC} - Should be at least 100,000 for high performance"
        echo "    Run: ulimit -n 1048576"
    else
        echo -e "  ${GREEN}✓ OK${NC}"
    fi
    
    if check_server; then
        FD_COUNT=$(lsof -p $SERVER_PID 2>/dev/null | wc -l)
        echo "  Current FDs used: $FD_COUNT"
    fi
}

# Quick performance test
quick_perf_test() {
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "Quick Performance Test (10 connections, 5 seconds)"
    echo "─────────────────────────────────────────────────────"
    
    if ! check_server; then
        return
    fi
    
    if ! command -v python3 &> /dev/null; then
        echo "  Python3 not found, skipping test"
        return
    fi
    
    # Create minimal test script
    cat > /tmp/quick_test.py << 'EOF'
import socket
import time

def test():
    results = []
    for _ in range(100):
        start = time.perf_counter()
        s = socket.socket()
        s.connect(('localhost', 8080))
        s.sendall(b'test\n')
        s.recv(1024)
        s.close()
        results.append(time.perf_counter() - start)
    
    avg = sum(results) / len(results)
    print(f"  100 requests completed")
    print(f"  Avg latency: {avg*1000:.2f}ms")
    print(f"  Estimated RPS: {1/avg:.0f}")
    
    if avg > 0.1:
        print(f"  ❌ SLOW - Latency >100ms indicates critical bug")
    elif avg > 0.01:
        print(f"  ⚠ MODERATE - Could be better")
    else:
        print(f"  ✓ FAST - Performance looks good")

test()
EOF
    
    python3 /tmp/quick_test.py
    rm /tmp/quick_test.py
}

# Check epoll event rate
check_epoll_events() {
    echo ""
    echo "─────────────────────────────────────────────────────"
    echo "Checking epoll Activity"
    echo "─────────────────────────────────────────────────────"
    
    if ! check_server; then
        return
    fi
    
    if ! command -v strace &> /dev/null; then
        echo "  strace not found, skipping"
        return
    fi
    
    echo "  Tracing epoll_wait for 2 seconds..."
    
    # Trace epoll_wait calls
    timeout 2 strace -p $SERVER_PID -e epoll_wait -c 2>&1 | tail -5 || true
}

# Summary and recommendations
print_summary() {
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo "  Diagnosis Summary"
    echo "═══════════════════════════════════════════════════════"
    echo ""
    
    ISSUES_FOUND=0
    
    # Check each issue
    if grep -q "write_sent = 0" thread_pool.cpp 2>/dev/null; then
        echo "🔴 CRITICAL: write_sent reset bug found"
        echo "   → Remove 'Conn.write_sent = 0' from event loop"
        echo ""
        ISSUES_FOUND=$((ISSUES_FOUND + 1))
    fi
    
    if grep -q "std::vector<Connection.*conn_pool" thread_pool.cpp 2>/dev/null; then
        echo "🟡 OPTIMIZE: Using vector for connection pool"
        echo "   → Switch to std::unordered_map<int, Connection>"
        echo ""
        ISSUES_FOUND=$((ISSUES_FOUND + 1))
    fi
    
    FD_LIMIT=$(ulimit -n)
    if [ $FD_LIMIT -lt 10000 ]; then
        echo "🟡 OPTIMIZE: Low file descriptor limit"
        echo "   → Run: ulimit -n 1048576"
        echo ""
        ISSUES_FOUND=$((ISSUES_FOUND + 1))
    fi
    
    if [ $ISSUES_FOUND -eq 0 ]; then
        echo "✅ No obvious bugs detected in code!"
        echo ""
        echo "If performance is still poor, try:"
        echo "  1. Profile with: perf record -g -p $SERVER_PID"
        echo "  2. Check kernel tuning"
        echo "  3. Verify network stack (lo interface for localhost tests)"
        echo "  4. Add debug logging to find bottleneck"
    else
        echo "Found $ISSUES_FOUND issue(s). Fix these first!"
        echo ""
        echo "After fixing, expected performance:"
        echo "  ✓ 100,000+ RPS with 100 connections"
        echo "  ✓ <1ms average latency"
        echo "  ✓ <5ms P99 latency"
    fi
    
    echo ""
}

# Main execution
main() {
    check_write_sent_bug
    check_vector_bug
    check_fd_limits
    check_cpu_usage
    quick_perf_test
    # check_epoll_events
    print_summary
}

main
