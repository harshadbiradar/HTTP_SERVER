#ifndef CONNECTION_H
#define CONNECTION_H

#include"common_header.h"

struct Connection
{
    int fd;
    std::string read_buffer;
    std::string write_buffer;
    struct epoll_event event;

    Connection(int Cl_fd, struct epoll_event Cl_event) : fd(Cl_fd), event(Cl_event)
    {}

    Connection(Connection &&other) noexcept
    {
        event = std::move(other.event);
        fd = other.fd;
        other.fd = -1;
        read_buffer = (std::move(other.read_buffer));
        write_buffer = (std::move(other.write_buffer));
    }
    ~Connection()
    {
        if (fd != -1)
        {
            // it means the object is out of scope, hence closing connection.
            // std::cout << "[INFO] Connection: " << fd << " no longer in use, closing it." << std::endl;
            close(fd);
        }
    }
};

#endif