#include"../include/Server.h"

void Server::terminate()
{
    //
    // also handle epoll_close,server_fd,event_fd close
    std::cout << "Shutting down thread pool" << std::endl;
    Worker_thread.shutdown();
    std::cout << "Closing server." << std::endl;
    close(server_socket);
    std::cout << "Closing epoll" << std::endl;
    close(epoll_fd);
    std::cout << "Closing eventfd" << std::endl;
    close(notify_fd);
    // std::cout << "Terminating program......" << std::endl;
    // std::terminate();
}

void Server::setup_server(){
    std::cout<<"Setting server"<<std::endl;
    sockaddr_in serverAddress;
    socklen_t len_sock;
    int retcode = 0;
    std::string str;

    // setting up server to user_specified port
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "[ERROR]: Error creating server_socket.\nTerminating program" << std::endl;
        terminate();
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[ERROR]: Error setsockopt() \nTerminating program" << std::endl;
        terminate();
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
        std::cerr << "[ERROR]: Error binding server_socket."<<port<<strerror(errno) << std::endl;
        terminate();
    }


    notify_fd = eventfd(0, EFD_NONBLOCK);
    if (notify_fd == -1)
    {
        std::cerr << "[ERROR]: Error creating eventfd." << std::endl;
        terminate();
    }

    // creating epoll_fd that will handle all the events we get.
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
        terminate();
    }

    // epoll event starts here.
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (retcode == -1)
    {
        std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
        terminate();
    }

    notify_event.events = EPOLLIN;
    notify_event.data.fd = notify_fd;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, notify_fd, &notify_event);
    if (retcode == -1)
    {
        std::cerr << "[ERROR]: Error adding event_fd in epoll_ctl." << std::endl;
        terminate();
    }

}


void Server::start(){
    std::cout<<"Starting server"<<std::endl;
    int listen_val = listen(server_socket, n_clients); // listening/holding 5 pending connections.
    if (listen_val == -1)
    {
        std::cerr << "[ERROR]: Error listening on server_socket." << std::endl;
        terminate();
    }
    running=true;

    while (running)
    {
        // std::cout << "In epoll wait" << std::endl;
        int n = epoll_wait(epoll_fd,events.data(), max_events, -1);
        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // accept all-- server event.
                conn_man.accept_all(server_socket,epoll_fd,events[i], live_connections);
            }
            else if (events[i].data.fd == notify_fd)
            {
                handle_notify_fd(notify_fd, live_connections);
            }
            else
            {
                handle_client_fd(events[i], live_connections);
            }
        }
        // sleep(1);
    }
}


void Server::handle_notify_fd(int notify_fd, std::unordered_map<int, std::shared_ptr<Connection>> &live_connections)
{
    uint64_t val;
    int retcode;

    if (read(notify_fd, &val, 8) == -1)
    {
        std::cerr << "[ERROR]: Error reading in eventfd: " << strerror(errno) << std::endl;
        terminate();
    }
    while (val--)
    {
        auto ready_pair = Writable_queue.try_pop();
        int temp_fd = ready_pair.first;
        // getting write buffer from second
        std::string temp_buffer = ready_pair.second;
        auto ref = live_connections.find(temp_fd);
        if (ref == live_connections.end())
        {
            // error here..
        }
        auto &my_conn = ref->second;
        my_conn->event.events = EPOLLOUT;
        my_conn->write_buffer.append(temp_buffer); // in both empty and non empty cases...
        // the above takes care of consequent writes..

        retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, temp_fd, &my_conn->event);
        if (retcode == -1)
        {
            std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
            terminate();
        }
    }
}

void Server::handle_client_fd(struct epoll_event &event,
                      std::unordered_map<int, std::shared_ptr<Connection>> &live_connections)
{
    // client event here.
    int retcode;
    // std::cout << "In post server_fd block wait" << std::endl;
    auto ref = live_connections.find(event.data.fd);
    if (ref == live_connections.end())
    {
        // error here..
    }
    auto &my_conn = ref->second;
    if (event.events & EPOLLIN)
    {
        // read_func(my_conn);
        // std::cout << "In recving block" << std::endl;
        int bytes_read;
        size_t old_size = my_conn->read_buffer.size();
        my_conn->read_buffer.resize(old_size + buff_len - 1);
        while ((bytes_read = recv(my_conn->fd, &my_conn->read_buffer[old_size], buff_len - 1, 0)) > 0)
        {
            // std::cout << "In while recving block with : " << bytes_read << std::endl;
            old_size += bytes_read;
            my_conn->read_buffer.resize(old_size + buff_len - 1);
        }
        // std::cout << "post while recving block with : " << bytes_read << std::endl;
        if (bytes_read == 0)
        {
            std::cout << "[INFO]: The Client has closed the connection, deleting this Connection." << std::endl;
            retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
            if (retcode == -1)
            {
                std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
                terminate();
            }
            close(my_conn->fd);
            live_connections.erase(ref);
            // handle post closed connection.
        }
        else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // std::cout << "In read_func calling block" << std::endl;
            my_conn->read_buffer.resize(old_size);
            Req_handler(event.data.fd, my_conn->read_buffer);
            my_conn->read_buffer.clear();
        }
    }
    else if (event.events & EPOLLOUT)
    {
        // std::cout << "In writing block" << std::endl;
        // write_func(my_conn);
        int bytes_sent = 0;
        // while (true)
        {
            bytes_sent = send(my_conn->fd, my_conn->write_buffer.c_str(),
                              my_conn->write_buffer.length(), MSG_NOSIGNAL);
            // std::cout<<"The debug: bytes sent: "<<bytes_sent<<std::endl;
            if (bytes_sent == -1)
            {
                if (errno == EINTR)
                {
                    bytes_sent = send(my_conn->fd, my_conn->write_buffer.c_str(),
                                      my_conn->write_buffer.length(), MSG_NOSIGNAL);
                    // continue;
                }
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                }
                else if (errno == EPIPE || errno == ECONNRESET)
                {
                    // connection broken.
                    std::cerr << "[ERROR]:" << strerror(errno) << " Client connection is closed.." << std::endl;
                    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
                    if (retcode == -1)
                    {
                        std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
                        terminate();
                    }
                    close(my_conn->fd);
                    live_connections.erase(ref);
                    // deleting that particular connection using refrence(iterator)
                }
                else
                {
                    std::cerr << "[ERROR]: failed with errno : " << strerror(errno) << std::endl;
                    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
                    if (retcode == -1)
                    {
                        std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
                        terminate();
                    }
                    close(my_conn->fd);
                    live_connections.erase(ref);
                }
            }
            else if (bytes_sent > 0)
            {
                my_conn->write_buffer.erase(0, bytes_sent);
            }

            if (my_conn->write_buffer.empty())
            {
                // std::cout << "[INFO] Write_buffer has been drained." << std::endl;
                my_conn->event.events = EPOLLIN;
                // since here it is confirmed data is sent completely.
                retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my_conn->fd, &my_conn->event);
                if (retcode == -1)
                {
                    std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
                    terminate();
                }
                // break;
            }
        }
    }
}


// will try to move the string resource, since it is temparory.
void Server::Req_handler(int fd, std::string &request)
{
    std::cout << "In Req_handler_func" << std::endl;
    auto lam = [request, fd,this]() mutable
    {
        std::cout << "In Req_handler_lambda" << std::endl;
        // Business_logic(request);
        Writable_queue.push(std::make_pair(fd, request));
        uint64_t value = 1;
        if (write(notify_fd, &value, sizeof(value)) == -1)
        {
            std::cerr << "[ERROR]: Error writing in eventfd: " << strerror(errno) << std::endl;
            terminate();
        }
    };
    Worker_thread.submit(lam);
}