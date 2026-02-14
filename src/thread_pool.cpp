#include "../include/Thread_pool.h"

void Thread_Pool::thread_func()
{
    // Pre-allocate a large vector to avoid frequent reallocations in the hot path.
    // Each thread gets its own copy. 65536 pointers = 512KB, very safe.
    std::vector<Connection *> all_connections(65536, nullptr);
    int server_socket = -1;
    int epoll_fd = -1;
    std::vector<epoll_event> events;
    struct epoll_event event;
    Connection *Conn;
    bool running = true;

    events.resize(Max_evnt_p_thd);

    setup_server(server_socket, epoll_fd, event);

    if (listen(server_socket, n_clients) == -1) {
        std::cerr << "Failed on listen" << std::endl;
        exit(1);
    }

    while (running)
    {
        int n = epoll_wait(epoll_fd, events.data(), Max_evnt_p_thd, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break; 
        }

        for (int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            if (fd == server_socket)
            {
                Conn_manager.accept_all(server_socket, epoll_fd, events[i]);
            }
            else
            {
                // Safety:  Dynamic resize if FD exceeds our pre-allocated vector
                if (fd >= (int)all_connections.size()) {
                    all_connections.resize(fd + 1024, nullptr);
                }

                Conn = all_connections[fd];
                if (Conn == nullptr)
                {
                    Conn = new Connection(fd, events[i]);
                    all_connections[fd] = Conn;
                }
                Conn_manager.handle_client_fd(epoll_fd, events[i], all_connections);
            }
        }
    }
}

void Thread_Pool::create_pool(int N)
{
    for (int i = 0; i < N; ++i)
    {
        pool.emplace_back(&Thread_Pool::thread_func, this);
    }
}

void Thread_Pool::close_pool()
{
    for (auto &t : pool)
    {
        if (t.joinable())
            t.join();
    }
}

void Thread_Pool::shutdown()
{
    {
        if (shutdown_flag)
            return;
        shutdown_flag = true;
    }
    close_pool();
}

void Thread_Pool::terminate(int &server_socket, int &epoll_fd)
{
    close(server_socket);
    close(epoll_fd);
    LOG_INFO("=== SERVER TERMINATED CLEANLY ===");
    std::terminate();
}

void Thread_Pool::setup_server(int &server_socket, int &epoll_fd, struct epoll_event &event)
{
    sockaddr_in serverAddress;
    int retcode = 0;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        LOG_ERROR("Failed to create server_socket: " << strerror(errno));
        terminate(server_socket, epoll_fd);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[ERROR]: Error setsockopt() \nTerminating program" << std::endl;
        terminate(server_socket, epoll_fd);
    }

    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int bind_val = bind(server_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bind_val == -1)
    {
        LOG_ERROR("Bind failed on port " << port << ": " << strerror(errno));
        terminate(server_socket, epoll_fd);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        LOG_ERROR("Error creating recv_epoll.\nTerminating program");
        terminate(server_socket, epoll_fd);
    }

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_socket;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (retcode == -1)
    {
        LOG_ERROR("Error adding socket_fd in epoll_ctl.");
        terminate(server_socket, epoll_fd);
    }
}
