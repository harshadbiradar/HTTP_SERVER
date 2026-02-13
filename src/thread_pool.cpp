#include "../include/Thread_pool.h"

void Thread_Pool::thread_func()
{
    // epoll_wait here....
    std::vector<Connection *>all_connections(n_clients+50,nullptr);
    int server_socket = -1;
    int epoll_fd = -1;
    std::vector<epoll_event> events;
    struct epoll_event event;
    Connection *Conn;
    bool running=true;

    events.reserve(Max_evnt_p_thd);

    setup_server(server_socket, epoll_fd, event);

    int listen_val=listen(server_socket,n_clients);
    if(listen_val==-1){
        std::cout<<"Failed on listen"<<std::endl;
        exit(1);
    }
    // int max_events;
    int n = 0;
    // std::cout<<"Waiting before while"<<std::endl;
    while (running || n)
    {
        // std::cout<<"Waiting at epoll_wait"<<std::endl;
        n = epoll_wait(epoll_fd, events.data(), Max_evnt_p_thd, -1);
        n = (Max_evnt_p_thd > n) ? n : Max_evnt_p_thd; // So wont endup handling, the extra loop(causing UB).
        for (int i = 0; i < n; i++)
        {
            if(events[i].data.fd==server_socket){
                Conn_manager.accept_all(server_socket,epoll_fd,events[i]);
            }
            else{
                if (all_connections[events[i].data.fd] == nullptr)
                {
                    Conn = new Connection(events[i].data.fd, events[i]);
                    all_connections[events[i].data.fd] = Conn;
                    // std::cout<<"allocated mate"<<std::endl;
                }
                Conn_manager.handle_client_fd(epoll_fd, events[i], all_connections);
            }
            
        }
    }
}

// template <size_t N>
void Thread_Pool::create_pool(int N)
{
    for (int i = 0; i < N; ++i)
    {
        pool.emplace_back(&Thread_Pool::thread_func, this);
    }
}

void Thread_Pool::close_pool()
{
    // std::cout << " [INFO]:: Terminating " << N << " Threads" << std::endl;
    for (auto &t : pool)
    {
        if (t.joinable())
            t.join();
        // //std::cout<<" Hello there"<<std::endl;
    }
}

// template <size_t N>

// template <size_t N>
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

    // std::cout << "Closing epoll" << std::endl;
    // LOG_INFO("Closing epoll_fd=" << recv_epoll);
    close(epoll_fd);

    // std::cout << "Closing eventfd" << std::endl;
    // LOG_INFO("Closing notify_fd=" << notify_fd);
    // close(notify_fd);

    // std::cout << "Terminating program......" << std::endl;
    LOG_INFO("=== SERVER TERMINATED CLEANLY ===");
    std::terminate();
}

void Thread_Pool::setup_server(int &server_socket, int &epoll_fd, struct epoll_event &event)
{
    sockaddr_in serverAddress;
    // socklen_t len_sock;
    int retcode = 0;

    // setting up server to user_specified port
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        // std::cerr << "[ERROR]: Error creating server_socket.\nTerminating program" << std::endl;
        LOG_ERROR("Failed to create server_socket: " << strerror(errno));
        terminate(server_socket, epoll_fd);
    }
    // LOG_INFO("Server socket created: fd=" << server_socket);

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[ERROR]: Error setsockopt() \nTerminating program" << std::endl;
        terminate(server_socket, epoll_fd);
    }

    // making it non_blocking, error handling yet to be done.
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int bind_val = bind(server_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bind_val == -1)
    {
        // std::cerr << "[ERROR]: Error binding server_socket."<<port<<strerror(errno) << std::endl;
        LOG_ERROR("Bind failed on port " << port << ": " << strerror(errno));
        terminate(server_socket, epoll_fd);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        // std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
        LOG_ERROR("Error creating recv_epoll.\nTerminating program");
        terminate(server_socket, epoll_fd);
    }
    // LOG_INFO("epoll instance created: recv_epoll=" << epoll_fd);

    // epoll event starts here.
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_socket;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (retcode == -1)
    {
        // std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
        LOG_ERROR("Error adding socket_fd in epoll_ctl.");
        terminate(server_socket, epoll_fd);
    }

    // Workers.create_pool(n_workers,dispacth_epoll);
}