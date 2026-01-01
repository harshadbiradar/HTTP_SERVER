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


    // notify_fd = eventfd(0, EFD_NONBLOCK);
    // if (notify_fd == -1)
    // {
    //     // std::cerr << "[ERROR]: Error creating eventfd." << std::endl;
    //     LOG_ERROR("Error creating eventfd.");
    //     terminate();
    // }
    // LOG_INFO("eventfd created: notify_fd=" << notify_fd);

    // creating epoll_fd that will handle all the events we get.
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        // std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
        LOG_ERROR("Error creating epoll_fd.\nTerminating program");
        terminate();
    }
    LOG_INFO("epoll instance created: epoll_fd=" << epoll_fd);

    for(int i=0;i<n_workers;++i){
        notify_fd = eventfd(0, EFD_NONBLOCK);
        if (notify_fd == -1)
        {
            // std::cerr << "[ERROR]: Error creating eventfd." << std::endl;
            LOG_ERROR("Error creating eventfd.");
            terminate();
        }
        notify_fds[i]=notify_fd;
        
        notify_events[i].events = EPOLLIN|EPOLLET;
        notify_events[i].data.fd = notify_fd;
        retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, notify_fd, &notify_events[i]);
        if (retcode == -1)
        {
            // std::cerr << "[ERROR]: Error adding event_fd in epoll_ctl." << std::endl;
            LOG_ERROR("Error adding event_fd in epoll_ctl.");
            terminate();
        }
        LOG_INFO("eventfd created: notify_fd=" << notify_fd);
    }


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

    // notify_event.events = EPOLLIN|EPOLLET;
    // notify_event.data.fd = notify_fd;
    // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, notify_fd, &notify_event);
    // if (retcode == -1)
    // {
    //     // std::cerr << "[ERROR]: Error adding event_fd in epoll_ctl." << std::endl;
    //     LOG_ERROR("Error adding event_fd in epoll_ctl.");
    //     terminate();
    // }

}


void Server::start(){
    static auto last_stats = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 10) {
        // LOG_INFO("STATS | Live connections: " << live_connections.size()
        //         << " | Writable queue backlog: " << Writable_queue.safe_size());
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
        //LOG_DEBUG("In epoll_wait");
        int n = epoll_wait(epoll_fd,events.data(), max_events, -1);
        for (int i = 0; i < n; i++)
        {   //LOG_DEBUG("In epoll_wait for "<<events[i].data.fd);
            if (events[i].data.fd == server_socket)
            {
                // accept all-- server event.
                conn_man.accept_all(server_socket,epoll_fd,events[i], live_connections);
            }
            else if (std::find(notify_fds.begin(), notify_fds.end(), events[i].data.fd) != notify_fds.end())
            {   
                //LOG_DEBUG("Going to notify_handle for "<<events[i].data.fd);
                handle_notify_fd(events[i].data.fd, live_connections);
            }
            else
            {   
                //LOG_DEBUG("Going to client_handle for "<<events[i].data.fd);
                handle_client_fd(events[i], live_connections);
            }
        }
        // sleep(1);
    }
}


// void Server::handle_notify_fd(int notify_fd, std::unordered_map<int, std::shared_ptr<Connection>> &live_connections)
// {
//     uint64_t val;
//     int retcode;
//     int index;
//     auto it=std::find(notify_fds.begin(),notify_fds.end(),notify_fd);
//     if(it!=notify_fds.end()){
//         index = std::distance(notify_fds.begin(), it);
//     }

//     if (read(notify_fd, &val, 8) == -1)
//     {
//         std::cerr << "[ERROR]: Error reading in eventfd: " << strerror(errno) << std::endl;
//         terminate();
//     }
//     while (val--)
//     {
//         auto ready_pair = write_queue_pool[index].try_pop();
//         int temp_fd = ready_pair.first;
//         // getting write buffer from second
//         std::string temp_buffer = ready_pair.second;
//         auto ref = live_connections.find(temp_fd);
//         if (ref == live_connections.end())
//         {
//             // error here..
//         }
//         auto &my_conn = ref->second;
//         my_conn->event.events = EPOLLOUT|EPOLLET;
//         my_conn->write_buffer.append(temp_buffer); // in both empty and non empty cases...
//         // the above takes care of consequent writes..

//         retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, temp_fd, &my_conn->event);
//         if (retcode == -1)
//         {
//             std::cerr << "[ERROR]: Some unexpected error, "<<strerror(errno)<<" Closing this particular connection..." << std::endl;
//             conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
            
//         }
//     }
// }

// void Server::handle_client_fd(struct epoll_event &event,
//                       std::unordered_map<int, std::shared_ptr<Connection>> &live_connections)
// {
//     // client event here.
//     int retcode;
//     // std::cout << "In post server_fd block wait" << std::endl;
//     auto ref = live_connections.find(event.data.fd);
//     if (ref == live_connections.end())
//     {
//         // error here..
//     }
//     auto &my_conn = ref->second;
//     if (event.events & EPOLLIN)
//     {
//         // read_func(my_conn);
//         // std::cout << "In recving block" << std::endl;
//         int bytes_read;
//         size_t old_size = my_conn->read_buffer.size();
//         my_conn->read_buffer.resize(old_size + buff_len - 1);
//         while ((bytes_read = recv(my_conn->fd, &my_conn->read_buffer[old_size], buff_len - 1, 0)) > 0)
//         {
//             // std::cout << "In while recving block with : " << bytes_read << std::endl;
//             old_size += bytes_read;
//             my_conn->read_buffer.resize(old_size + buff_len - 1);
//         }
//         ////LOG_DEBUG("Read " << bytes_read << " bytes from fd=" << my_conn->fd << " | buffer size now: " << my_conn->read_buffer.size());
//         // std::cout << "post while recving block with : " << bytes_read << std::endl;
//         if (bytes_read == 0)
//         {
//             // std::cout << "[INFO]: The Client has closed the connection, deleting this Connection." << std::endl;
//             // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
//             // if (retcode == -1)
//             // {
//             //     std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
//             //     terminate();
//             // }
//             // close(my_conn->fd);
//             // live_connections.erase(ref);
//             ////LOG_DEBUG("Client closed connection: fd=" << my_conn->fd);
//             conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);

//             // handle post closed connection.
//         }
//         else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
//         {
//             // std::cout << "In read_func calling block" << std::endl;
//             ////LOG_DEBUG("Request queued for processing: fd=" << my_conn->fd);
//             my_conn->read_buffer.resize(old_size);
//             Req_handler(event.data.fd, std::move(my_conn->read_buffer));
//             my_conn->read_buffer.clear();
//         }
//     }
//     else if (event.events & EPOLLOUT)
//     {
//         // std::cout << "In writing block" << std::endl;
//         // write_func(my_conn);
//         int bytes_sent = 0;
//         // while (true)
//         {
//             bytes_sent = send(my_conn->fd, my_conn->write_buffer.c_str(),
//                               my_conn->write_buffer.length(), MSG_NOSIGNAL);
//             ////LOG_DEBUG("Sent " << bytes_sent << " bytes to fd=" << my_conn->fd << " | write_buffer remaining: " << my_conn->write_buffer.size());
//             // std::cout<<"The debug: bytes sent: "<<bytes_sent<<std::endl;
//             if (bytes_sent == -1)
//             {
//                 if (errno == EINTR)
//                 {
//                     bytes_sent = send(my_conn->fd, my_conn->write_buffer.c_str(),
//                                       my_conn->write_buffer.length(), MSG_NOSIGNAL);
//                     // continue;
//                 }
//                 else if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 {
//                 }
//                 else if (errno == EPIPE || errno == ECONNRESET)
//                 {
//                     // connection broken.
//                     std::cerr << "[ERROR]:" << strerror(errno) << " Client connection is closed.." << std::endl;
//                     // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
//                     // if (retcode == -1)
//                     // {
//                     //     std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
//                     //     terminate();
//                     // }
//                     // close(my_conn->fd);
//                     // live_connections.erase(ref);
//                     conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
//                     // deleting that particular connection using refrence(iterator)
//                 }
//                 else
//                 {
//                     std::cerr << "[ERROR]: failed with errno : " << strerror(errno) << std::endl;
//                     // retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, my_conn->fd, nullptr);
//                     // if (retcode == -1)
//                     // {
//                     //     std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
//                     //     terminate();
//                     // }
//                     // close(my_conn->fd);
//                     // live_connections.erase(ref);
//                     conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
//                 }
//             }
//             else if (bytes_sent > 0)
//             {
//                 my_conn->write_buffer.erase(0, bytes_sent);
//             }

//             if (my_conn->write_buffer.empty())
//             {
//                 // std::cout << "[INFO] Write_buffer has been drained." << std::endl;
//                 ////LOG_DEBUG("Write buffer drained, switching to EPOLLIN: fd=" << my_conn->fd);
//                 my_conn->event.events = EPOLLIN|EPOLLET;
//                 // since here it is confirmed data is sent completely.
//                 retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my_conn->fd, &my_conn->event);
//                 if (retcode == -1)
//                 {
//                     std::cerr << "[ERROR]: Some unexpected error while modifying, "<<strerror(errno)<<" Closing this particular connection..." << std::endl;
//                     conn_man.close_connection(my_conn->fd,epoll_fd,live_connections);
//                 }
//                 // break;
//             }
//         }
//     }
// }


// will try to move the string resource, since it is temparory.
// void Server::Req_handler(int fd, std::string &&request)
// {
//     ////LOG_DEBUG("Req_handler called for fd=" << fd << " | request size=" << request.size() << " bytes");
//     // std::cout << "In Req_handler_func" << std::endl;
//     std::string response;
//     size_t header_end = request.find("\r\n\r\n");
//     if (header_end != std::string::npos) {
//         // Extract body (everything after headers)
//         std::string body = request.substr(header_end + 4);

//         // Build minimal valid HTTP response
//         response = "HTTP/1.1 200 OK\r\n"
//                    "Content-Type: text/plain\r\n"
//                    "Content-Length: " + std::to_string(body.size()) + "\r\n"
//                    "Connection: close\r\n"
//                    "\r\n" +
//                    body;  // Echo the body back
//     } else {
//         // Fallback: if no proper headers, just raw echo (for your C client)
//         response = std::move(request);
//     }


//     auto lam = [response=std::move(response), fd,this]() mutable
//     {
//         // std::cout << "In Req_handler_lambda" << std::endl;
//         ////LOG_DEBUG("Worker thread processing request from fd=" << fd << " | size=" << request.size());
//         int write_val=-1;
//         // Business_logic(request);
//         // Writable_queue.push(std::make_pair(fd, response));
//         write_queue_pool[fd%n_workers].push(std::make_pair(fd, response));
//         uint64_t value = 1;
//         do{
//             write_val=write(notify_fds[fd%n_workers], &value, sizeof(value));
//         }while(write_val==-1&&errno==EINTR);
//         if (write_val == -1)
//         {
//             // std::cerr << "[ERROR]: Error writing in eventfd: " << strerror(errno) << std::endl;
//             LOG_ERROR("Failed to write to notify_fd: " << strerror(errno));
//             terminate();
//         }
//         else {
//             ////LOG_DEBUG("Successfully queued response and signaled notify_fd for fd=" << fd);
//         }
//     };
//     Worker_thread.submit(fd%n_workers,lam);
// }

// void Server::Req_handler(int fd, std::shared_ptr<Connection> conn) {
//     // 1. Prepare Response (Zero-copy approach)
//     std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nHello";
//     conn->read_idx = 0; // Reset read buffer for next request

//     // 2. Offload to Worker
//     auto lam = [response = std::move(response), fd, this]() mutable {
//         bool was_empty = write_queue_pool[fd % n_workers].push(std::make_pair(fd, std::move(response)));
        
//         // 3. Syscall Batching: Only notify if the queue was empty
//         if (was_empty) {
//             uint64_t value = 1;
//             write(notify_fds[fd % n_workers], &value, 8);
//         }
//     };
//     Worker_thread.submit(fd % n_workers, lam);
// }

// void Server::handle_notify_fd(int n_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections) {
//     uint64_t u;
//     read(n_fd, &u, sizeof(uint64_t)); // Clear eventfd signal

//     int q_idx = -1;
//     for(int i=0; i<n_workers; ++i) if(notify_fds[i] == n_fd) q_idx = i;
//     if (q_idx == -1) return;

//     // 4. THE DRAIN LOOP: Process EVERYTHING currently in the queue
//     while (true) {
//         auto [success, data] = write_queue_pool[q_idx].try_pop();
//         if (!success) break;

//         int client_fd = data.first;
//         auto it = Live_connections.find(client_fd);
//         if (it != Live_connections.end()) {
//             send(client_fd, data.second.data(), data.second.size(), 0);
//         }
//     }
// }

// void Server::handle_notify_fd(int n_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections) {
//     uint64_t u;
//     if (read(n_fd, &u, sizeof(uint64_t)) < 0) return;

//     // Identify which worker queue triggered this
//     int idx = -1;
//     for(int i=0; i < n_workers; ++i) {
//         if(notify_fds[i] == n_fd) { idx = i; break; }
//     }
//     if (idx == -1) return;

//     // DRAIN LOOP: This prevents the "0 requests" hang
//     while (true) {
//         auto [success, item] = write_queue_pool[idx].try_pop();
//         if (!success) break; 

//         int fd = item.first;
//         auto it = Live_connections.find(fd);
//         if (it != Live_connections.end()) {
//             // In a real high-perf server, you'd check for EAGAIN here too
//             send(fd, item.second.c_str(), item.second.size(), 0);
//         }
//     }
// }



void Server::handle_notify_fd(int n_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections) {
    uint64_t u;
    // Clear the eventfd notification
    //LOG_DEBUG("In handle_notify_fd");
    if (read(n_fd, &u, sizeof(uint64_t)) <= 0) return;

    // Identify which worker queue triggered this
    size_t sent;
    int q_idx = -1;
    for(int i=0; i < n_workers; ++i) {
        if(notify_fds[i] == n_fd) { q_idx = i; break; }
    }
    if (q_idx == -1) return;


    int processed=0;
    int MAX_BATCH=512;
    
    // THE DRAIN LOOP
    // We must process ALL items currently in the queue because 'was_empty'
    // optimization might have skipped several write() calls while we were working.
    while (processed<MAX_BATCH) {
        //LOG_DEBUG("-----------In handle_notify_fd __poping loop");
        auto [success, item] = write_queue_pool[q_idx].try_pop();
        if (!success) break; 

        int client_fd = item.first;
        auto it = Live_connections.find(client_fd);
        if (it != Live_connections.end()) {
            sent=send(client_fd, item.second.c_str(), item.second.size(), 0);
            if(sent==-1){
                if (errno == EPIPE || errno == ECONNRESET || errno == EBADF) {
                    // BROKEN PIPE: The client is gone. Clean up now.
                    conn_man.close_connection(client_fd, epoll_fd, Live_connections);
                }
            }
            
        }
    }
}

void Server::Req_handler(int fd, std::shared_ptr<Connection> conn) {
    // Zero-copy response
    //LOG_DEBUG("In Req_handler");
    // std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
    // conn->read_buffer.clear();
    // conn->read_idx=0;

    auto lam = [ fd, this]() mutable {
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
        int idx = fd % n_workers;
        
        // Push and check if we need to signal
        bool was_empty = write_queue_pool[idx].push(std::make_pair(fd, std::move(response)));
        
        if (was_empty) {
            uint64_t val = 1;
            write(notify_fds[idx], &val, 8);
        }
    };
    Worker_thread.submit(fd % n_workers, lam);
}


void Server::handle_client_fd(struct epoll_event &event,
                             std::unordered_map<int, std::shared_ptr<Connection>> &live_connections)
{
    // 1. Efficient Lookup
    //LOG_DEBUG("In handle_client");
    auto it = live_connections.find(event.data.fd);
    if (it == live_connections.end()) return;
    auto &my_conn = it->second;

    if (event.events & EPOLLIN)
    {
        // 2. Loop until EAGAIN (Required for Edge-Triggered mode)
        while (true)
        {   
            //LOG_DEBUG("In Read till EAGAIN loop...");
            // Calculate remaining space in the pre-allocated read_buffer
            int space_left = my_conn->read_buffer.size() - my_conn->read_idx;
            
            // If buffer is full, we must either handle the error or expand
            if (space_left <= 0) {
                // For high performance, we avoid reallocation, but for robustness:
                my_conn->read_buffer.resize(my_conn->read_buffer.size() * 2);
                space_left = my_conn->read_buffer.size() - my_conn->read_idx;
            }

            // 3. Raw recv into the specific offset of the vector
            ssize_t bytes_read = recv(my_conn->fd, 
                                     &my_conn->read_buffer[my_conn->read_idx], 
                                     space_left, 0);

            if (bytes_read > 0)
            {   
                //LOG_DEBUG("Bytes recieved... "<<bytes_read);
                my_conn->read_idx += bytes_read;
                // std::string_view current_data(&my_conn->read_buffer[0], my_conn->read_idx);
                // if (current_data.find('\n') != std::string_view::npos) {
                //     Req_handler(my_conn->fd, my_conn);
                // }
                while (true) { // Loop to process multiple requests in one buffer
                    std::string_view current_data(&my_conn->read_buffer[0], my_conn->read_idx);
                    auto pos = current_data.find('\n');
                    
                    if (pos != std::string_view::npos) {
                        Req_handler(my_conn->fd, my_conn); // Logic below
                        
                        // SHIFT the leftover data to the front
                        size_t processed_len = pos + 1;
                        if (my_conn->read_idx > processed_len) {
                            memmove(&my_conn->read_buffer[0], 
                                    &my_conn->read_buffer[processed_len], 
                                    my_conn->read_idx - processed_len);
                            my_conn->read_idx -= processed_len;
                        } else {
                            my_conn->read_idx = 0;
                            break; // No more data to process
                        }
                    } else {
                        break; // No complete request found yet
                    }
                }

            }
            else if (bytes_read == -1)
            {
                //LOG_DEBUG("Bytes recieved... "<<bytes_read);
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Kernel buffer is empty, stop reading and wait for next epoll trigger
                    break;
                } else if (errno == EINTR) {
                    // Interrupted by signal, try again immediately
                    continue;
                } else {
                    // Actual error
                    conn_man.close_connection(my_conn->fd, epoll_fd, live_connections);
                    return;
                }
            }
            else // bytes_read == 0 (Client closed connection)
            {
                //LOG_DEBUG("Bytes recieved... "<<bytes_read);
                conn_man.close_connection(my_conn->fd, epoll_fd, live_connections);
                return;
            }
        }
    }
    else if (event.events & EPOLLOUT)
    {
        // 5. Optimized Write Path
        // Send from the write_buffer starting at write_sent offset
        size_t total_to_send = my_conn->write_buffer.size() - my_conn->write_sent;
        if (total_to_send > 0) {
            ssize_t bytes_sent = send(my_conn->fd, 
                                     &my_conn->write_buffer[my_conn->write_sent], 
                                     total_to_send, 0);
            
            if (bytes_sent > 0) {
                my_conn->write_sent += bytes_sent;
                if (my_conn->write_sent == my_conn->write_buffer.size()) {
                    // Done writing! Switch back to watching for inputs
                    my_conn->write_buffer.clear();
                    my_conn->write_sent = 0;
                    
                    struct epoll_event ev = my_conn->event;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my_conn->fd, &ev);
                }
            }
        }
    }
}