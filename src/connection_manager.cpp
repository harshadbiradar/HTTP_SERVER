#include "../include/Connection_manager.h"

static const std::string HTTP_RESPONSE = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "Hello World\r\n";

void Connection_manager::accept_all(int sock_fd, int epoll_fd, struct epoll_event &event)
{
    int client_fd;
    sockaddr_in serverAddress;
    socklen_t len_sock = sizeof(serverAddress);

    while (true)
    {
        client_fd = accept(sock_fd, (struct sockaddr *)&serverAddress, &len_sock);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return;
        }

        // Disable Nagle's algorithm for immediate packet transmission
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        // Set non-blocking
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        // Register for READ events initially
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
        {
            close(client_fd);
        }
    }
}

void Connection_manager::close_connection(int fd, int epoll_fd, std::vector<Connection*> &all_connections)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    if (all_connections[fd] != nullptr) {
        delete all_connections[fd];
        all_connections[fd] = nullptr;
    }
    close(fd);
}

void Connection_manager::remove_all_connection(int epoll_fd, std::vector<Connection*> &all_connections)
{
    for (size_t i = 0; i < all_connections.size(); ++i) {
        if (all_connections[i] != nullptr) {
            close_connection(i, epoll_fd, all_connections);
        }
    }
}

void Connection_manager::handle_client_fd(int epoll_fd, struct epoll_event &event,
                             std::vector<Connection*> &all_connections)
{
    Connection* Conn = all_connections[event.data.fd];
    if (!Conn) return;

    // Update local event cache
    Conn->event.events = event.events;

    if (event.events & EPOLLIN)
    {
        while (true)
        {   
            int space_left = Conn->read_buffer.size() - Conn->read_idx;
            if (space_left <= 0) {
                Conn->read_buffer.resize(Conn->read_buffer.size() * 2);
                space_left = Conn->read_buffer.size() - Conn->read_idx;
            }

            ssize_t bytes_read = recv(Conn->fd, &Conn->read_buffer[Conn->read_idx], space_left, 0);

            if (bytes_read > 0)
            {   
                Conn->read_idx += bytes_read;
                
                while (true) {
                    std::string_view current_data(&Conn->read_buffer[0], Conn->read_idx);
                    auto pos = current_data.find("\n");
                    
                    if (pos != std::string_view::npos) {
                        Conn->write_buffer.insert(Conn->write_buffer.end(), 
                                                HTTP_RESPONSE.begin(), 
                                                HTTP_RESPONSE.end());
                        
                        size_t processed_len = pos + 1;
                        if (Conn->read_idx > processed_len) {
                            memmove(&Conn->read_buffer[0], &Conn->read_buffer[processed_len], 
                                    Conn->read_idx - processed_len);
                            Conn->read_idx -= processed_len;
                        } else {
                            Conn->read_idx = 0;
                            break; 
                        }
                    } else {
                        break; 
                    }
                }
            }
            else if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else if (errno == EINTR) continue;
                else {
                    close_connection(Conn->fd, epoll_fd, all_connections);
                    return;
                }
            }
            else {
                close_connection(Conn->fd, epoll_fd, all_connections);
                return;
            }
        }
        
        if (!Conn->write_buffer.empty()) {
            try_write(Conn, epoll_fd, all_connections);
        }
    }
    else if (event.events & EPOLLOUT)
    {
        try_write(Conn, epoll_fd, all_connections);
    }
}

void Connection_manager::try_write(Connection* Conn, int epoll_fd, 
                                   std::vector<Connection*> &all_connections)
{
    while (Conn->write_sent < Conn->write_buffer.size())
    {
        size_t remaining = Conn->write_buffer.size() - Conn->write_sent;
        ssize_t bytes_sent = send(Conn->fd, &Conn->write_buffer[Conn->write_sent], remaining, 0);
        
        if (bytes_sent > 0) {
            Conn->write_sent += bytes_sent;
        }
        else if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Buffer full: Register for EPOLLOUT if not already registered
                if (!(Conn->event.events & EPOLLOUT)) {
                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                    ev.data.fd = Conn->fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &ev);
                    Conn->event.events = ev.events;
                }
                return;
            }
            else if (errno == EINTR) continue;
            else {
                close_connection(Conn->fd, epoll_fd, all_connections);
                return;
            }
        }
    }
    
    // Success: Fully sent
    Conn->write_buffer.clear();
    Conn->write_sent = 0;
    
    // Syscall optimization: Only MOD back to EPOLLIN if we previously enabled EPOLLOUT
    if (Conn->event.events & EPOLLOUT) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = Conn->fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &ev);
        Conn->event.events = ev.events;
    }
}
