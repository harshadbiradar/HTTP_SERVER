#ifndef CONNECTION_H
#define CONNECTION_H

#include "common_header.h"
#include <vector>

// 8KB is enough for standard HTTP headers/tiny packets
const size_t MAX_BUF = 16384; 

struct Connection {
    int fd;
    
    // Pre-allocated buffers
    std::vector<char> read_buffer;
    size_t read_idx = 0;

    std::vector<char> write_buffer;
    size_t write_idx = 0;
    size_t write_sent = 0;

    struct epoll_event event;

    Connection(int Cl_fd, struct epoll_event Cl_event) : fd(Cl_fd), event(Cl_event) {
        read_buffer.resize(MAX_BUF);
    }

    Connection(Connection &&other) noexcept {
        event = other.event;
        fd = other.fd;
        other.fd = -1;
        read_buffer = std::move(other.read_buffer);
        read_idx = other.read_idx;
        write_buffer = std::move(other.write_buffer);
        write_idx = other.write_idx;
        write_sent = other.write_sent;
    }

    ~Connection() {
        // if (fd != -1) close(fd);
    }
};

#endif