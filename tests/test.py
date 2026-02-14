# # echo_benchmark.py
# import asyncio
# import time
# import sys

# HOST = '127.0.0.1'
# PORT = 8080
# MESSAGE = b"ECHO" * 100 + b"\n"  # 500-byte message, ends with \n for easy counting
# CONCURRENT_CONNECTIONS = 500    # Start low, we'll ramp up
# DURATION = 30                   # seconds

# async def echo_client(client_id):
#     try:
#         reader, writer = await asyncio.open_connection(HOST, PORT)
#         sent = 0
#         received = 0

#         start_time = time.time()
#         while time.time() - start_time < DURATION:
#             writer.write(MESSAGE)
#             await writer.drain()
#             sent += 1

#             data = await asyncio.wait_for(reader.read(4096), timeout=10.0)
#             if not data:
#                 break
#             received += len(data)

#         writer.close()
#         await writer.wait_closed()
#         return sent, received
#     except Exception as e:
#         print(f"Client {client_id} error: {e}")
#         return 0, 0

# async def main():
#     print(f"Starting benchmark: {CONCURRENT_CONNECTIONS} concurrent connections, {DURATION}s duration")
#     print(f"Message size: {len(MESSAGE)} bytes\n")

#     start = time.time()
#     tasks = [echo_client(i) for i in range(CONCURRENT_CONNECTIONS)]
#     results = await asyncio.gather(*tasks)
#     total_time = time.time() - start

#     total_sent = sum(r[0] for r in results)
#     total_received = sum(r[1] for r in results)

#     req_per_sec = total_sent / total_time
#     throughput_mbps = (total_received * 8) / (total_time * 1_000_000)

#     print(f"\nResults:")
#     print(f"   Duration:          {total_time:.2f} seconds")
#     print(f"   Connections:       {CONCURRENT_CONNECTIONS}")
#     print(f"   Total requests:    {total_sent}")
#     print(f"   Requests/sec:      {req_per_sec:,.0f}")
#     print(f"   Throughput:        {throughput_mbps:.2f} Mbps")
#     print(f"   Avg msg latency:   {total_time / total_sent * 1000:.2f} ms (rough)")

# if __name__ == "__main__":
#     if len(sys.argv) > 1:
#         CONCURRENT_CONNECTIONS = int(sys.argv[1])
#     if len(sys.argv) > 2:
#         DURATION = int(sys.argv[2])

#     asyncio.run(main())



# #!/usr/bin/env python3
# """
# TCP/UDP Load Tester - Measures RPS and Latency under Concurrent Connections
# Usage: python3 load_tester.py --host localhost --port 8080
# """

# import socket
# import time
# import asyncio
# import argparse
# import statistics
# import sys
# from dataclasses import dataclass
# from typing import List
# import json


# @dataclass
# class TestResult:
#     connections: int
#     total_requests: int
#     duration: float
#     rps: float
#     errors: int
#     latencies: List[float]
    
#     def get_percentile(self, p: float) -> float:
#         if not self.latencies:
#             return 0
#         sorted_lat = sorted(self.latencies)
#         idx = int(len(sorted_lat) * p)
#         return sorted_lat[min(idx, len(sorted_lat) - 1)]
    
#     def summary(self):
#         return {
#             'connections': self.connections,
#             'total_requests': self.total_requests,
#             'duration': f"{self.duration:.2f}s",
#             'rps': f"{self.rps:.2f}",
#             'errors': self.errors,
#             'latency_min': f"{min(self.latencies) * 1000:.2f}ms" if self.latencies else "N/A",
#             'latency_max': f"{max(self.latencies) * 1000:.2f}ms" if self.latencies else "N/A",
#             'latency_avg': f"{statistics.mean(self.latencies) * 1000:.2f}ms" if self.latencies else "N/A",
#             'latency_p50': f"{self.get_percentile(0.50) * 1000:.2f}ms",
#             'latency_p95': f"{self.get_percentile(0.95) * 1000:.2f}ms",
#             'latency_p99': f"{self.get_percentile(0.99) * 1000:.2f}ms",
#         }


# class LoadTester:
#     def __init__(self, host: str, port: int, protocol: str = 'tcp'):
#         self.host = host
#         self.port = port
#         self.protocol = protocol.lower()
        
#     async def send_request(self, reader, writer, message: bytes, terminator: bytes) -> float:
#         """Send a single request and measure latency"""
#         start = time.perf_counter()
        
#         try:
#             writer.write(message)
#             await writer.drain()
            
#             response = await reader.readuntil(terminator)
            
#             latency = time.perf_counter() - start
#             return latency
#         except Exception as e:
#             raise e
    
#     async def connection_worker(self, conn_id: int, num_requests: int, 
#                                 message: bytes, terminator: bytes,
#                                 results: dict):
#         """Worker that maintains one connection and sends multiple requests"""
#         latencies = []
#         errors = 0
        
#         try:
#             reader, writer = await asyncio.open_connection(self.host, self.port)
            
#             for i in range(num_requests):
#                 try:
#                     latency = await self.send_request(reader, writer, message, terminator)
#                     latencies.append(latency)
#                 except Exception as e:
#                     errors += 1
#                     if errors > num_requests * 0.1:
#                         break
            
#             writer.close()
#             await writer.wait_closed()
            
#         except Exception as e:
#             print(f"Connection {conn_id} failed: {e}", file=sys.stderr)
#             errors += num_requests
        
#         results['latencies'].extend(latencies)
#         results['errors'] += errors
#         results['completed'] += len(latencies)
    
#     async def run_phase(self, num_connections: int, requests_per_conn: int,
#                        message: str, terminator: str) -> TestResult:
#         """Run one test phase with specified number of concurrent connections"""
        
#         msg_bytes = message.encode()
#         term_bytes = terminator.encode()
        
#         results = {
#             'latencies': [],
#             'errors': 0,
#             'completed': 0
#         }
        
#         start_time = time.perf_counter()
        
#         tasks = [
#             self.connection_worker(i, requests_per_conn, msg_bytes, term_bytes, results)
#             for i in range(num_connections)
#         ]
        
#         await asyncio.gather(*tasks, return_exceptions=True)
        
#         duration = time.perf_counter() - start_time
        
#         total_requests = results['completed']
#         rps = total_requests / duration if duration > 0 else 0
        
#         return TestResult(
#             connections=num_connections,
#             total_requests=total_requests,
#             duration=duration,
#             rps=rps,
#             errors=results['errors'],
#             latencies=results['latencies']
#         )
    
#     async def run_test(self, start_conns: int, max_conns: int, step: int,
#                       requests_per_conn: int, message: str, terminator: str):
#         """Run full load test across multiple concurrency levels"""
        
#         print(f"\n{'='*80}")
#         print(f"Load Test Configuration:")
#         print(f"  Target: {self.host}:{self.port} ({self.protocol.upper()})")
#         print(f"  Concurrency: {start_conns} -> {max_conns} (step: {step})")
#         print(f"  Requests per connection: {requests_per_conn}")
#         print(f"  Message: {repr(message)}")
#         print(f"{'='*80}\n")
        
#         all_results = []
        
#         current_conns = start_conns
#         while current_conns <= max_conns:
#             print(f"\nTesting with {current_conns} concurrent connections...")
            
#             try:
#                 result = await self.run_phase(current_conns, requests_per_conn, message, terminator)
#                 all_results.append(result)
                
#                 summary = result.summary()
#                 print(f"   RPS: {summary['rps']} | "
#                       f"P95: {summary['latency_p95']} | "
#                       f"Errors: {result.errors}/{result.total_requests}")
                
#             except Exception as e:
#                 print(f"   Phase failed: {e}")
#                 break
            
#             current_conns += step
            
#             await asyncio.sleep(0.5)
        
#         return all_results
    
#     def print_summary(self, results: List[TestResult]):
#         """Print final summary table"""
        
#         print(f"\n{'='*80}")
#         print("LOAD TEST RESULTS")
#         print(f"{'='*80}")
#         print(f"{'Conns':<8} {'RPS':<12} {'Avg':<10} {'P50':<10} {'P95':<10} {'P99':<10} {'Errors':<8}")
#         print(f"{'-'*80}")
        
#         for r in results:
#             s = r.summary()
#             print(f"{r.connections:<8} "
#                   f"{s['rps']:<12} "
#                   f"{s['latency_avg']:<10} "
#                   f"{s['latency_p50']:<10} "
#                   f"{s['latency_p95']:<10} "
#                   f"{s['latency_p99']:<10} "
#                   f"{r.errors:<8}")
        
#         print(f"{'='*80}\n")
        
#         if results:
#             max_rps_result = max(results, key=lambda x: x.rps)
#             print(f"Peak Performance: {max_rps_result.rps:.2f} RPS "
#                   f"at {max_rps_result.connections} connections\n")


# def main():
#     parser = argparse.ArgumentParser(description='TCP/UDP Load Tester')
#     parser.add_argument('--host', default='localhost', help='Server host')
#     parser.add_argument('--port', type=int, required=True, help='Server port')
#     parser.add_argument('--protocol', default='tcp', choices=['tcp', 'udp'], help='Protocol')
#     parser.add_argument('--start-conns', type=int, default=1000, help='Starting concurrent connections')
#     parser.add_argument('--max-conns', type=int, default=10000, help='Maximum concurrent connections')
#     parser.add_argument('--step', type=int, default=1000, help='Connection increment step')
#     parser.add_argument('--requests-per-conn', type=int, default=1000, help='Requests per connection')
#     parser.add_argument('--message', default='PING\n', help='Message to send')
#     parser.add_argument('--terminator', default='\n', help='Response terminator')
#     parser.add_argument('--output', help='Output results to JSON file')
    
#     args = parser.parse_args()
    
#     tester = LoadTester(args.host, args.port, args.protocol)
    
#     try:
#         results = asyncio.run(tester.run_test(
#             args.start_conns,
#             args.max_conns,
#             args.step,
#             args.requests_per_conn,
#             args.message,
#             args.terminator
#         ))
        
#         tester.print_summary(results)
        
#         if args.output:
#             with open(args.output, 'w') as f:
#                 json.dump([r.summary() for r in results], f, indent=2)
#             print(f"Results exported to {args.output}")
            
#     except KeyboardInterrupt:
#         print("\n\nTest interrupted by user")
#         sys.exit(1)


# if __name__ == '__main__':
#     main()



#!/usr/bin/env python3
"""
TCP Load Tester - Time-Based Sustained Load Testing
Each connection sends as many requests as possible for a fixed duration
Usage: python3 load_tester.py --host localhost --port 8080 --duration 30
"""

import socket
import time
import asyncio
import argparse
import statistics
import sys
from dataclasses import dataclass
from typing import List
import json


@dataclass
class TestResult:
    connections: int
    total_requests: int
    duration: float
    rps: float
    errors: int
    latencies: List[float]
    
    def get_percentile(self, p: float) -> float:
        if not self.latencies:
            return 0
        sorted_lat = sorted(self.latencies)
        idx = int(len(sorted_lat) * p)
        return sorted_lat[min(idx, len(sorted_lat) - 1)]
    
    def summary(self):
        return {
            'connections': self.connections,
            'total_requests': self.total_requests,
            'duration': f"{self.duration:.2f}s",
            'rps': f"{self.rps:.2f}",
            'errors': self.errors,
            'latency_min': f"{min(self.latencies) * 1000:.2f}ms" if self.latencies else "N/A",
            'latency_max': f"{max(self.latencies) * 1000:.2f}ms" if self.latencies else "N/A",
            'latency_avg': f"{statistics.mean(self.latencies) * 1000:.2f}ms" if self.latencies else "N/A",
            'latency_p50': f"{self.get_percentile(0.50) * 1000:.2f}ms",
            'latency_p95': f"{self.get_percentile(0.95) * 1000:.2f}ms",
            'latency_p99': f"{self.get_percentile(0.99) * 1000:.2f}ms",
        }


class LoadTester:
    def __init__(self, host: str, port: int, protocol: str = 'tcp'):
        self.host = host
        self.port = port
        self.protocol = protocol.lower()
        
    async def send_request(self, reader, writer, message: bytes, terminator: bytes) -> float:
        """Send a single request and measure latency"""
        start = time.perf_counter()
        
        try:
            writer.write(message)
            await writer.drain()
            
            response = await asyncio.wait_for(reader.readuntil(terminator), timeout=5.0)
            
            latency = time.perf_counter() - start
            return latency
        except Exception as e:
            raise e
    
    async def connection_worker(self, conn_id: int, duration: float,
                                message: bytes, terminator: bytes,
                                results: dict, stop_event: asyncio.Event):
        """Worker that hammers requests for the specified duration"""
        latencies = []
        errors = 0
        requests_sent = 0
        
        try:
            reader, writer = await asyncio.open_connection(self.host, self.port)
            
            start_time = time.perf_counter()
            
            while not stop_event.is_set():
                elapsed = time.perf_counter() - start_time
                if elapsed >= duration:
                    break
                
                try:
                    latency = await self.send_request(reader, writer, message, terminator)
                    latencies.append(latency)
                    requests_sent += 1
                except asyncio.TimeoutError:
                    print(f"Connection {conn_id} timed out waiting for response", file=sys.stderr)
                    errors += 1
                    if errors > 10:
                        break
                except Exception as e:
                    print(f"Connection {conn_id} error: {type(e).__name__} - {e}", file=sys.stderr)
                    errors += 1
                    if errors > 10:
                        break
            
            writer.close()
            await writer.wait_closed()
            
        except Exception as e:
            print(f"Connection {conn_id} failed to establish: {e}", file=sys.stderr)
            errors += 1
        
        results['latencies'].extend(latencies)
        results['errors'] += errors
        results['completed'] += requests_sent
    
    async def run_phase(self, num_connections: int, duration: float,
                       message: str, terminator: str) -> TestResult:
        """Run one test phase with specified number of concurrent connections for fixed duration"""
        
        msg_bytes = message.encode()
        term_bytes = terminator.encode()
        
        results = {
            'latencies': [],
            'errors': 0,
            'completed': 0
        }
        
        stop_event = asyncio.Event()
        
        # print(f"   Opening {num_connections} connections...")
        sys.stdout.flush()
        
        start_time = time.perf_counter()
        
        tasks = [
            self.connection_worker(i, duration, msg_bytes, term_bytes, results, stop_event)
            for i in range(num_connections)
        ]
        
        # print(f"   Running load test for {duration} seconds...")
        sys.stdout.flush()
        
        await asyncio.gather(*tasks, return_exceptions=True)
        
        actual_duration = time.perf_counter() - start_time
        
        total_requests = results['completed']
        rps = total_requests / actual_duration if actual_duration > 0 else 0
        
        return TestResult(
            connections=num_connections,
            total_requests=total_requests,
            duration=actual_duration,
            rps=rps,
            errors=results['errors'],
            latencies=results['latencies']
        )
    
    async def run_test(self, connection_levels: List[int], duration: float,
                      message: str, terminator: str):
        """Run full load test across multiple concurrency levels"""
        
        print(f"\n{'='*80}")
        print(f"Load Test Configuration:")
        print(f"  Target: {self.host}:{self.port} ({self.protocol.upper()})")
        print(f"  Test duration per phase: {duration} seconds")
        print(f"  Connection levels: {connection_levels}")
        print(f"  Message: {repr(message)}")
        print(f"  Strategy: Each connection sends as many requests as possible")
        print(f"{'='*80}\n")
        
        all_results = []
        
        for num_connections in connection_levels:
            print(f"\n{'='*80}")
            print(f"Phase: {num_connections} concurrent connections")
            print(f"{'='*80}")
            
            try:
                result = await self.run_phase(num_connections, duration, message, terminator)
                all_results.append(result)
                
                summary = result.summary()
                print(f"\n   Results:")
                print(f"      Total Requests: {result.total_requests:,}")
                print(f"      RPS: {summary['rps']}")
                print(f"      Avg Latency: {summary['latency_avg']}")
                print(f"      P50 Latency: {summary['latency_p50']}")
                print(f"      P95 Latency: {summary['latency_p95']}")
                print(f"      P99 Latency: {summary['latency_p99']}")
                print(f"      Errors: {result.errors}")
                
            except Exception as e:
                print(f"   Phase failed: {e}")
                import traceback
                traceback.print_exc()
                break
            
            if num_connections != connection_levels[-1]:
                print(f"\n   Cooling down for 2 seconds...")
                await asyncio.sleep(2)
        
        return all_results
    
    def print_summary(self, results: List[TestResult]):
        """Print final summary table"""
        
        print(f"\n{'='*80}")
        print("LOAD TEST SUMMARY")
        print(f"{'='*80}")
        print(f"{'Conns':<8} {'RPS':<12} {'Reqs':<10} {'Avg':<10} {'P50':<10} {'P95':<10} {'P99':<10} {'Err':<6}")
        print(f"{'-'*80}")
        
        for r in results:
            s = r.summary()
            print(f"{r.connections:<8} "
                  f"{s['rps']:<12} "
                  f"{r.total_requests:<10} "
                  f"{s['latency_avg']:<10} "
                  f"{s['latency_p50']:<10} "
                  f"{s['latency_p95']:<10} "
                  f"{s['latency_p99']:<10} "
                  f"{r.errors:<6}")
        
        print(f"{'='*80}\n")
        
        if results:
            max_rps_result = max(results, key=lambda x: x.rps)
            print(f"Peak Performance:")
            print(f"  {max_rps_result.rps:.2f} RPS at {max_rps_result.connections} connections")
            print(f"  ({max_rps_result.total_requests:,} requests in {max_rps_result.duration:.2f}s)\n")


def main():
    parser = argparse.ArgumentParser(
        description='TCP Load Tester - Time-based sustained load testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test with single connection level for 30 seconds
  python3 load_tester.py --port 8080 --connections 100 --duration 30

  # Test multiple connection levels
  python3 load_tester.py --port 8080 --connections 10,50,100,200,500 --duration 30

  # Quick test with short duration
  python3 load_tester.py --port 8080 --connections 10,100,500 --duration 10
        """
    )
    
    parser.add_argument('--host', default='localhost', help='Server host (default: localhost)')
    parser.add_argument('--port', type=int, required=True, help='Server port')
    parser.add_argument('--protocol', default='tcp', choices=['tcp', 'udp'], help='Protocol (default: tcp)')
    parser.add_argument('--connections', type=str, default='10,50,100,200,500', 
                       help='Comma-separated connection levels to test (default: 10,50,100,200,500)')
    parser.add_argument('--duration', type=int, default=30, 
                       help='Duration in seconds for each phase (default: 30)')
    parser.add_argument('--message', default='PING\n', help='Message to send (default: PING\\n)')
    parser.add_argument('--terminator', default='\n', help='Response terminator (default: \\n)')
    parser.add_argument('--output', help='Output results to JSON file')
    
    args = parser.parse_args()
    
    connection_levels = [int(x.strip()) for x in args.connections.split(',')]
    connection_levels.sort()
    
    tester = LoadTester(args.host, args.port, args.protocol)
    
    try:
        results = asyncio.run(tester.run_test(
            connection_levels,
            args.duration,
            args.message,
            args.terminator
        ))
        
        tester.print_summary(results)
        
        if args.output:
            with open(args.output, 'w') as f:
                json.dump([r.summary() for r in results], f, indent=2)
            print(f"Results exported to {args.output}")
            
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(1)


if __name__ == '__main__':
    main()