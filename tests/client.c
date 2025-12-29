// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <pthread.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <time.h>
// #include <stdint.h>

// #define HOST "127.0.0.1"
// #define PORT 8080
// #define MSG_SIZE 401  // 400 'E' + '\n'

// char message[MSG_SIZE];

// typedef struct {
//     int connections;
//     int duration;
//     uint64_t total_sent;
//     uint64_t total_received;
// } bench_result_t;

// void* client_thread(void* arg) {
//     bench_result_t* result = (bench_result_t*)arg;
//     int sock;
//     struct sockaddr_in server;
//     uint64_t sent = 0;
//     uint64_t received = 0;
//     time_t end_time = time(NULL) + result->duration;

//     char recv_buf[4096];

//     while (time(NULL) < end_time) {
//         sock = socket(AF_INET, SOCK_STREAM, 0);
//         if (sock < 0) continue;

//         server.sin_addr.s_addr = inet_addr(HOST);
//         server.sin_family = AF_INET;
//         server.sin_port = htons(PORT);

//         if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
//             close(sock);
//             continue;
//         }

//         // Send message
//         if (send(sock, message, MSG_SIZE, 0) <= 0) {
//             close(sock);
//             continue;
//         }
//         sent++;

//         // Receive echo
//         ssize_t n = recv(sock, recv_buf, sizeof(recv_buf), 0);
//         if (n > 0) received += n;

//         close(sock);
//     }

//     __atomic_add_fetch(&result->total_sent, sent, __ATOMIC_RELAXED);
//     __atomic_add_fetch(&result->total_received, received, __ATOMIC_RELAXED);
//     return NULL;
// }

// int main(int argc, char* argv[]) {
//     if (argc != 3) {
//         fprintf(stderr, "Usage: %s <connections> <duration_seconds>\n", argv[0]);
//         return 1;
//     }

//     int connections = atoi(argv[1]);
//     int duration = atoi(argv[2]);

//     if (connections <= 0 || duration <= 0) {
//         fprintf(stderr, "Connections and duration must be > 0\n");
//         return 1;
//     }

//     // Prepare message: 400 'E' + newline
//     memset(message, 'E', 400);
//     message[400] = '\n';

//     bench_result_t result = { .connections = connections, .duration = duration, .total_sent = 0, .total_received = 0 };

//     pthread_t* threads = malloc(connections * sizeof(pthread_t));
//     if (!threads) {
//         perror("malloc");
//         return 1;
//     }

//     printf("Starting benchmark: %d concurrent connections, %ds duration\n", connections, duration);
//     printf("Message size: %d bytes\n\n", MSG_SIZE);

//     double start_time = (double)clock() / CLOCKS_PER_SEC;

//     for (int i = 0; i < connections; i++) {
//         pthread_create(&threads[i], NULL, client_thread, &result);
//     }

//     for (int i = 0; i < connections; i++) {
//         pthread_join(threads[i], NULL);
//     }

//     double elapsed = (double)(clock() / CLOCKS_PER_SEC) - start_time;

//     double req_per_sec = result.total_sent / elapsed;
//     double throughput_mbps = (result.total_received * 8.0) / (elapsed * 1000000.0);
//     double avg_latency_ms = (result.total_sent > 0) ? (elapsed / result.total_sent * 1000.0) : 0.0;

//     printf("Results:\n");
//     printf("   Duration:          %.2f seconds\n", elapsed);
//     printf("   Connections:       %d\n", connections);
//     printf("   Total requests:    %llu\n", (unsigned long long)result.total_sent);
//     printf("   Requests/sec:      %.0f\n", req_per_sec);
//     printf("   Throughput:        %.2f Mbps\n", throughput_mbps);
//     printf("   Avg msg latency:   %.2f ms (rough)\n", avg_latency_ms);

//     free(threads);
//     return 0;
// }


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

#define HOST "127.0.0.1"
#define PORT 8080
#define MSG_SIZE 401                    // 400 'E' + '\n'
#define THREAD_STACK_SIZE (1024 * 1024) // 1 MB per thread — safe on 8GB RAM

char message[MSG_SIZE];

typedef struct {
    int connections;
    int duration_sec;
    uint64_t total_sent;
    uint64_t total_received;
    struct timespec end_time;
} bench_params_t;

void* client_thread(void* arg) {
    bench_params_t* params = (bench_params_t*)arg;
    int sock;
    struct sockaddr_in server;
    uint64_t sent = 0;
    uint64_t received = 0;
    char recv_buf[4096];
    struct timespec now;

    server.sin_addr.s_addr = inet_addr(HOST);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > params->end_time.tv_sec ||
            (now.tv_sec == params->end_time.tv_sec && now.tv_nsec >= params->end_time.tv_nsec)) {
            break;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            close(sock);
            continue;
        }

        if (send(sock, message, MSG_SIZE, 0) <= 0) {
            close(sock);
            continue;
        }
        sent++;

        ssize_t n = recv(sock, recv_buf, sizeof(recv_buf), 0);
        if (n > 0) received += n;

        close(sock);
    }

    __atomic_add_fetch(&params->total_sent, sent, __ATOMIC_RELAXED);
    __atomic_add_fetch(&params->total_received, received, __ATOMIC_RELAXED);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <connections> <duration_seconds>\n", argv[0]);
        return 1;
    }

    int connections = atoi(argv[1]);
    int duration_sec = atoi(argv[2]);

    if (connections <= 0 || duration_sec <= 0) {
        fprintf(stderr, "Connections and duration must be > 0\n");
        return 1;
    }

    // Prepare message
    memset(message, 'E', 400);
    message[400] = '\n';

    bench_params_t params = {
        .connections = connections,
        .duration_sec = duration_sec,
        .total_sent = 0,
        .total_received = 0
    };

    // Set precise end time
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    params.end_time.tv_sec = start.tv_sec + duration_sec;
    params.end_time.tv_nsec = start.tv_nsec;

    pthread_t* threads = malloc(connections * sizeof(pthread_t));
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

    printf("Starting benchmark: %d concurrent connections, %ds duration\n", connections, duration_sec);
    printf("Message size: %d bytes\n", MSG_SIZE);
    printf("Thread stack size: 1 MB\n\n");

    // Create threads
    int created = 0;
    for (int i = 0; i < connections; i++) {
        if (pthread_create(&threads[i], &attr, client_thread, &params) != 0) {
            fprintf(stderr, "Warning: Failed to create thread %d (stopping at %d threads)\n", i, i);
            created = i;
            break;
        }
        created = i + 1;
    }

    // Join all created threads
    for (int i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
    }

    // Calculate real elapsed time
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    double req_per_sec = params.total_sent / elapsed;
    double throughput_mbps = (params.total_received * 8.0) / (elapsed * 1000000.0);
    double avg_latency_ms = (params.total_sent > 0) ? (elapsed / params.total_sent * 1000.0) : 0.0;

    printf("Results:\n");
    printf("   Duration:          %.2f seconds\n", elapsed);
    printf("   Connections:       %d (created)\n", created);
    printf("   Total requests:    %llu\n", (unsigned long long)params.total_sent);
    printf("   Requests/sec:      %'.0f\n", req_per_sec);
    printf("   Throughput:        %.2f Mbps\n", throughput_mbps);
    printf("   Avg msg latency:   %.2f ms (rough)\n", avg_latency_ms);

    pthread_attr_destroy(&attr);
    free(threads);
    return 0;
}