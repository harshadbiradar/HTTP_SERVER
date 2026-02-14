#!/usr/bin/env python3
"""
TCP Configuration Tester
Tests various TCP configurations to find performance bottleneck
"""

import socket
import time
import statistics

def test_config(host, port, nodelay=False, quickack=False, num_requests=100):
    """Test with specific TCP configuration"""
    latencies = []
    
    for _ in range(num_requests):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            
            if nodelay:
                s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            
            # TCP_QUICKACK is Linux-specific
            if quickack:
                try:
                    TCP_QUICKACK = 12  # Linux constant
                    s.setsockopt(socket.IPPROTO_TCP, TCP_QUICKACK, 1)
                except:
                    pass
            
            s.connect((host, port))
            
            start = time.perf_counter()
            s.sendall(b'test\n')
            response = s.recv(100)
            latency = (time.perf_counter() - start) * 1000
            
            s.close()
            latencies.append(latency)
            
        except Exception as e:
            print(f"Error: {e}")
            continue
    
    if not latencies:
        return None
    
    return {
        'avg': statistics.mean(latencies),
        'min': min(latencies),
        'max': max(latencies),
        'p50': statistics.median(latencies),
        'p95': sorted(latencies)[int(len(latencies) * 0.95)],
        'rps': 1000 / statistics.mean(latencies)
    }

def main():
    host = 'localhost'
    port = 8080
    
    print("="*70)
    print("TCP Configuration Performance Test")
    print("="*70)
    print(f"Target: {host}:{port}")
    print(f"Requests per config: 100")
    print("")
    
    configs = [
        ("Default (Nagle enabled)", False, False),
        ("TCP_NODELAY only", True, False),
        ("TCP_NODELAY + TCP_QUICKACK", True, True),
    ]
    
    results = []
    
    for name, nodelay, quickack in configs:
        print(f"Testing: {name}...")
        result = test_config(host, port, nodelay, quickack)
        
        if result:
            results.append((name, result))
            print(f"  ✓ Avg: {result['avg']:.2f}ms, RPS: {result['rps']:.0f}")
        else:
            print(f"  ✗ Failed")
        print("")
    
    # Summary
    print("="*70)
    print("Summary")
    print("="*70)
    print(f"{'Config':<35} {'Avg Latency':<15} {'RPS':<12} {'Improvement'}")
    print("-"*70)
    
    baseline_rps = results[0][1]['rps'] if results else 0
    
    for name, result in results:
        improvement = f"{result['rps'] / baseline_rps:.1f}x" if baseline_rps > 0 else "-"
        print(f"{name:<35} {result['avg']:>7.2f}ms       {result['rps']:>10.0f}  {improvement:>8}")
    
    print("="*70)
    print("")
    
    # Diagnosis
    if results:
        default = results[0][1]
        nodelay = results[1][1] if len(results) > 1 else None
        
        if nodelay and nodelay['rps'] > default['rps'] * 5:
            print("🔴 DIAGNOSIS: Nagle's algorithm is your bottleneck!")
            print("   → Server's TCP_NODELAY is NOT working")
            print("   → Fix: Ensure setsockopt(TCP_NODELAY) is called after accept()")
            print("")
        elif nodelay and nodelay['rps'] > default['rps'] * 1.5:
            print("🟡 DIAGNOSIS: Nagle's algorithm has some impact")
            print("   → TCP_NODELAY helps but isn't the main issue")
            print("")
        else:
            print("🟢 DIAGNOSIS: TCP configuration looks OK")
            print("   → Bottleneck is elsewhere (check system_debugging.md)")
            print("")
        
        if default['avg'] > 100:
            print("⚠️  WARNING: Extremely high latency detected (>100ms)")
            print("   Possible causes:")
            print("   1. Server is CPU bound (check with 'top')")
            print("   2. Thread contention or locks")
            print("   3. System resource limits (ulimit -n)")
            print("   4. Network configuration issues")
            print("")

if __name__ == '__main__':
    main()