#include"../include/Server.h"

void Server::terminate()
{
    //
    // also handle epoll_close,server_fd,event_fd close
    LOG_INFO("=== TERMINATE CALLED ===");
    LOG_INFO("Active connections before shutdown: " << live_connections.size());

    // std::cout << "Shutting down thread pool" << std::endl;
    LOG_INFO("Shutting down thread pool");
    Worker_thread.shutdown();

    // std::cout<<"Removing all active connections"<<std::endl;
    LOG_INFO("Removing all active connections");
    conn_man.remove_all_connection(epoll_fd,live_connections);

    // std::cout << "Closing server." << std::endl;
    LOG_INFO("Closing server_socket=" << server_socket);
    close(server_socket);

    // std::cout << "Closing epoll" << std::endl;
    LOG_INFO("Closing epoll_fd=" << epoll_fd);
    close(epoll_fd);

    // std::cout << "Closing eventfd" << std::endl;
    LOG_INFO("Closing notify_fd=" << notify_fd);
    close(notify_fd);

    // std::cout << "Terminating program......" << std::endl;
    LOG_INFO("=== SERVER TERMINATED CLEANLY ===");
    std::terminate();
}

void Server::setup_server(){
    LOG_INFO("=== SERVER STARTUP ===");
    LOG_INFO("Configuration: port=" << port 
             << " | workers=" << n_workers 
             << " | max_events=" << max_events 
             << " | queue_size=" << queue_size 
             << " | buff_len=" << buff_len);
    // std::cout<<"Setting server"<<std::endl;
    sockaddr_in serverAddress;
    // socklen_t len_sock;
    int retcode = 0;
    std::string str;

    // setting up server to user_specified port
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        // std::cerr << "[ERROR]: Error creating server_socket.\nTerminating program" << std::endl;
        LOG_ERROR("Failed to create server_socket: " << strerror(errno));
        terminate();
    }
    LOG_INFO("Server socket created: fd=" << server_socket);

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
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
        // std::cerr << "[ERROR]: Error binding server_socket."<<port<<strerror(errno) << std::endl;
        LOG_ERROR("Bind failed on port " << port << ": " << strerror(errno));
        terminate();
    }


    notify_fd = eventfd(0, EFD_NONBLOCK);
    if (notify_fd == -1)
    {
        // std::cerr << "[ERROR]: Error creating eventfd." << std::endl;
        LOG_ERROR("Error creating eventfd.");
        terminate();
    }
    LOG_INFO("eventfd created: notify_fd=" << notify_fd);

    // creating epoll_fd that will handle all the events we get.
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        // std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
        LOG_ERROR("Error creating epoll_fd.\nTerminating program");
        terminate();
    }
    LOG_INFO("epoll instance created: epoll_fd=" << epoll_fd);

    // epoll event starts here.
    event.events = EPOLLIN|EPOLLET;
    event.data.fd = server_socket;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (retcode == -1)
    {
        // std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
        LOG_ERROR("Error adding socket_fd in epoll_ctl.");
        terminate();
    }

    notify_event.events = EPOLLIN|EPOLLET;
    notify_event.data.fd = notify_fd;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, notify_fd, &notify_event);
    if (retcode == -1)
    {
        // std::cerr << "[ERROR]: Error adding event_fd in epoll_ctl." << std::endl;
        LOG_ERROR("Error adding event_fd in epoll_ctl.");
        terminate();
    }

}


void Server::start(){
    static auto last_stats = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 10) {
        LOG_INFO("STATS | Live connections: " << live_connections.size()
                << " | Writable queue backlog: " << Writable_queue.safe_size());
        last_stats = now;
    }
    std::cout<<"Starting server"<<std::endl;
    int listen_val=-1;
    // int listen_val = listen(server_socket, n_clients); // listening/holding 5 pending connections.
    // if (listen_val == -1)
    // {
    //     std::cerr << "[ERROR]: Error listening on server_socket." << std::endl;
    //     terminate();//to not terminate, wait for few sec..
    // }
    do{
        //staying here till we get any connection, and not abruptly terminating.
        listen_val = listen(server_socket, n_clients); // listening/holding 5 pending connections.
    }while(listen_val==-1);
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
        my_conn->event.events = EPOLLOUT|EPOLLET;
        my_conn->write_buffer.append(temp_buffer); // in both empty and non empty cases...
        // the above takes care of consequent writes..

        retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, temp_fd, &my_conn->event);
        if (retcode == -1)
        {
            std::cerr << "[ERROR]: Some unexpected error, "<<strerror(errno)<<" Closing this particular connection..." << std::endl;
            conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
            
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
        //LOG_DEBUG("Read " << bytes_read << " bytes from fd=" << my_conn->fd << " | buffer size now: " << my_conn->read_buffer.size());
        // std::cout << "post while recving block with : " << bytes_read << std::endl;
        if (bytes_read == 0)
        {
            // std::cout << "[INFO]: The Client has closed the connection, deleting this Connection." << std::endl;
            // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
            // if (retcode == -1)
            // {
            //     std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
            //     terminate();
            // }
            // close(my_conn->fd);
            // live_connections.erase(ref);
            //LOG_DEBUG("Client closed connection: fd=" << my_conn->fd);
            conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);

            // handle post closed connection.
        }
        else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // std::cout << "In read_func calling block" << std::endl;
            //LOG_DEBUG("Request queued for processing: fd=" << my_conn->fd);
            my_conn->read_buffer.resize(old_size);
            Req_handler(event.data.fd, std::move(my_conn->read_buffer));
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
            //LOG_DEBUG("Sent " << bytes_sent << " bytes to fd=" << my_conn->fd << " | write_buffer remaining: " << my_conn->write_buffer.size());
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
                    // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
                    // if (retcode == -1)
                    // {
                    //     std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
                    //     terminate();
                    // }
                    // close(my_conn->fd);
                    // live_connections.erase(ref);
                    conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
                    // deleting that particular connection using refrence(iterator)
                }
                else
                {
                    std::cerr << "[ERROR]: failed with errno : " << strerror(errno) << std::endl;
                    // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
                    // if (retcode == -1)
                    // {
                    //     std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
                    //     terminate();
                    // }
                    // close(my_conn->fd);
                    // live_connections.erase(ref);
                    conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
                }
            }
            else if (bytes_sent > 0)
            {
                my_conn->write_buffer.erase(0, bytes_sent);
            }

            if (my_conn->write_buffer.empty())
            {
                // std::cout << "[INFO] Write_buffer has been drained." << std::endl;
                //LOG_DEBUG("Write buffer drained, switching to EPOLLIN: fd=" << my_conn->fd);
                my_conn->event.events = EPOLLIN|EPOLLET;
                // since here it is confirmed data is sent completely.
                retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my_conn->fd, &my_conn->event);
                if (retcode == -1)
                {
                    std::cerr << "[ERROR]: Some unexpected error while modifying, "<<strerror(errno)<<" Closing this particular connection..." << std::endl;
                    conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
                }
                // break;
            }
        }
    }
}


// will try to move the string resource, since it is temparory.
void Server::Req_handler(int fd, std::string &&request)
{
    //LOG_DEBUG("Req_handler called for fd=" << fd << " | request size=" << request.size() << " bytes");
    // std::cout << "In Req_handler_func" << std::endl;
    std::string response;
    size_t header_end = request.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        // Extract body (everything after headers)
        std::string body = request.substr(header_end + 4);

        // Build minimal valid HTTP response
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: " + std::to_string(body.size()) + "\r\n"
                   "Connection: close\r\n"
                   "\r\n" +
                   body;  // Echo the body back
    } else {
        // Fallback: if no proper headers, just raw echo (for your C client)
        response = std::move(request);
    }


    auto lam = [response=std::move(response), fd,this]() mutable
    {
        // std::cout << "In Req_handler_lambda" << std::endl;
        //LOG_DEBUG("Worker thread processing request from fd=" << fd << " | size=" << request.size());
        int write_val=-1;
        // Business_logic(request);
        Writable_queue.push(std::make_pair(fd, response));
        uint64_t value = 1;
        do{
            write_val=write(notify_fd, &value, sizeof(value));
        }while(write_val==-1&&errno==EINTR);
        if (write_val == -1)
        {
            // std::cerr << "[ERROR]: Error writing in eventfd: " << strerror(errno) << std::endl;
            LOG_ERROR("Failed to write to notify_fd: " << strerror(errno));
            terminate();
        }
        else {
            //LOG_DEBUG("Successfully queued response and signaled notify_fd for fd=" << fd);
        }
    };
    Worker_thread.submit(lam);
}