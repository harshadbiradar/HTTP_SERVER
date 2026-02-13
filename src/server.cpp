#include"../include/Server.h"

void Server::terminate()
{
    //
    // also handle epoll_close,server_fd,event_fd close
    LOG_INFO("=== TERMINATE CALLED ===");
    LOG_INFO("Active connections before shutdown: " << live_connections.size());

    // std::cout << "Shutting down thread pool" << std::endl;
    // LOG_INFO("Shutting down thread pool");
    // Worker_thread.shutdown();

    // std::cout<<"Removing all active connections"<<std::endl;
    // LOG_INFO("Removing all active connections");
    // conn_man.remove_all_connection(epoll_fd,live_connections);

    // std::cout << "Closing server." << std::endl;
    LOG_INFO("Closing server_socket=" << server_socket);
    close(server_socket);

    // std::cout << "Closing epoll" << std::endl;
    LOG_INFO("Closing epoll_fd=" << recv_epoll);
    close(recv_epoll);

    LOG_INFO("Closing epoll_fd=" << dispacth_epoll);
    close(dispacth_epoll);

    // std::cout << "Closing eventfd" << std::endl;
    // LOG_INFO("Closing notify_fd=" << notify_fd);
    // close(notify_fd);

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

    recv_epoll = epoll_create1(0);
    if (recv_epoll == -1)
    {
        // std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
        LOG_ERROR("Error creating recv_epoll.\nTerminating program");
        terminate();
    }
    LOG_INFO("epoll instance created: recv_epoll=" << recv_epoll);
    // dispacth_epoll = epoll_create1(0);
    // if (dispacth_epoll == -1)
    // {
    //     // std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
    //     LOG_ERROR("Error creating dispacth_epoll.\nTerminating program");
    //     terminate();
    // }
    // LOG_INFO("epoll instance created: dispacth_epoll=" << dispacth_epoll);


    // epoll event starts here.
    event.events = EPOLLIN|EPOLLET;
    event.data.fd = server_socket;
    retcode = epoll_ctl(recv_epoll, EPOLL_CTL_ADD, server_socket, &event);
    if (retcode == -1)
    {
        // std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
        LOG_ERROR("Error adding socket_fd in epoll_ctl.");
        terminate();
    }

    // Workers.create_pool(n_workers,dispacth_epoll);

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



    // do{
    //     //staying here till we get any connection, and not abruptly terminating.
    //     listen_val = listen(server_socket, n_clients); // listening/holding 5 pending connections.
    // }while(listen_val==-1);

    listen_val=listen(server_socket,n_clients);
    if(listen_val==-1){
        std::cout<<"Failed on listen"<<std::endl;
        exit(1);
    }

    running=true;

    while (running)
    {
        // std::cout << "In epoll wait" << std::endl;
        //LOG_DEBUG("In epoll_wait");
        int n = epoll_wait(recv_epoll,events.data(), max_events, -1);
        for (int i = 0; i < n; i++)
        {   //LOG_DEBUG("In epoll_wait for "<<events[i].data.fd);
            if (events[i].data.fd == server_socket)
            {
                // accept all-- server event.
                conn_man.accept_all(server_socket,recv_epoll,dispacth_epoll,events[i]);
            }
        }
        // sleep(1);
    }
}

// void Server::handle_notify_fd(int n_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections) {
//     uint64_t u;
//     // Clear the eventfd notification
//     //LOG_DEBUG("In handle_notify_fd");
//     if (read(n_fd, &u, sizeof(uint64_t)) <= 0) return;

//     // Identify which worker queue triggered this
//     size_t sent;
//     int q_idx = -1;
//     for(int i=0; i < n_workers; ++i) {
//         if(notify_fds[i] == n_fd) { q_idx = i; break; }
//     }
//     if (q_idx == -1) return;


//     int processed=0;
//     int MAX_BATCH=512;
    
//     // THE DRAIN LOOP
//     // We must process ALL items currently in the queue because 'was_empty'
//     // optimization might have skipped several write() calls while we were working.
//     while (processed<MAX_BATCH) {
//         //LOG_DEBUG("-----------In handle_notify_fd __poping loop");
//         auto [success, item] = write_queue_pool[q_idx].try_pop();
//         if (!success) break; 

//         int client_fd = item.first;
//         auto it = Live_connections.find(client_fd);
//         if (it != Live_connections.end()) {
//             sent=send(client_fd, item.second.c_str(), item.second.size(), 0);
//             if(sent==-1){
//                 if (errno == EPIPE || errno == ECONNRESET || errno == EBADF) {
//                     // BROKEN PIPE: The client is gone. Clean up now.
//                     conn_man.close_connection(client_fd, epoll_fd, Live_connections);
//                 }
//             }
            
//         }
//     }
// }

// void Server::Req_handler(int fd, std::shared_ptr<Connection> conn) {
//     // Zero-copy response
//     //LOG_DEBUG("In Req_handler");
//     // std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
//     // conn->read_buffer.clear();
//     // conn->read_idx=0;

//     auto lam = [ fd, this]() mutable {
//         std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
//         int idx = fd % n_workers;
        
//         // Push and check if we need to signal
//         bool was_empty = write_queue_pool[idx].push(std::make_pair(fd, std::move(response)));
        
//         if (was_empty) {
//             uint64_t val = 1;
//             write(notify_fds[idx], &val, 8);
//         }
//     };
//     Worker_thread.submit(fd % n_workers, lam);
// }


// void Server::handle_client_fd(struct epoll_event &event,
//                              std::unordered_map<int, std::shared_ptr<Connection>> &live_connections)
// {
//     // 1. Efficient Lookup
//     //LOG_DEBUG("In handle_client");
//     auto it = live_connections.find(event.data.fd);
//     if (it == live_connections.end()) return;
//     auto &my_conn = it->second;

//     if (event.events & EPOLLIN)
//     {
//         // 2. Loop until EAGAIN (Required for Edge-Triggered mode)
//         while (true)
//         {   
//             //LOG_DEBUG("In Read till EAGAIN loop...");
//             // Calculate remaining space in the pre-allocated read_buffer
//             int space_left = my_conn->read_buffer.size() - my_conn->read_idx;
            
//             // If buffer is full, we must either handle the error or expand
//             if (space_left <= 0) {
//                 // For high performance, we avoid reallocation, but for robustness:
//                 my_conn->read_buffer.resize(my_conn->read_buffer.size() * 2);
//                 space_left = my_conn->read_buffer.size() - my_conn->read_idx;
//             }

//             // 3. Raw recv into the specific offset of the vector
//             ssize_t bytes_read = recv(my_conn->fd, 
//                                      &my_conn->read_buffer[my_conn->read_idx], 
//                                      space_left, 0);

//             if (bytes_read > 0)
//             {   
//                 //LOG_DEBUG("Bytes recieved... "<<bytes_read);
//                 my_conn->read_idx += bytes_read;
//                 // std::string_view current_data(&my_conn->read_buffer[0], my_conn->read_idx);
//                 // if (current_data.find('\n') != std::string_view::npos) {
//                 //     Req_handler(my_conn->fd, my_conn);
//                 // }
//                 while (true) { // Loop to process multiple requests in one buffer
//                     std::string_view current_data(&my_conn->read_buffer[0], my_conn->read_idx);
//                     auto pos = current_data.find('\n');
                    
//                     if (pos != std::string_view::npos) {
//                         Req_handler(my_conn->fd, my_conn); // Logic below
                        
//                         // SHIFT the leftover data to the front
//                         size_t processed_len = pos + 1;
//                         if (my_conn->read_idx > processed_len) {
//                             memmove(&my_conn->read_buffer[0], 
//                                     &my_conn->read_buffer[processed_len], 
//                                     my_conn->read_idx - processed_len);
//                             my_conn->read_idx -= processed_len;
//                         } else {
//                             my_conn->read_idx = 0;
//                             break; // No more data to process
//                         }
//                     } else {
//                         break; // No complete request found yet
//                     }
//                 }

//             }
//             else if (bytes_read == -1)
//             {
//                 //LOG_DEBUG("Bytes recieved... "<<bytes_read);
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // Kernel buffer is empty, stop reading and wait for next epoll trigger
//                     break;
//                 } else if (errno == EINTR) {
//                     // Interrupted by signal, try again immediately
//                     continue;
//                 } else {
//                     // Actual error
//                     conn_man.close_connection(my_conn->fd, epoll_fd, live_connections);
//                     return;
//                 }
//             }
//             else // bytes_read == 0 (Client closed connection)
//             {
//                 //LOG_DEBUG("Bytes recieved... "<<bytes_read);
//                 conn_man.close_connection(my_conn->fd, epoll_fd, live_connections);
//                 return;
//             }
//         }
//     }
//     else if (event.events & EPOLLOUT)
//     {
//         // 5. Optimized Write Path
//         // Send from the write_buffer starting at write_sent offset
//         size_t total_to_send = my_conn->write_buffer.size() - my_conn->write_sent;
//         if (total_to_send > 0) {
//             ssize_t bytes_sent = send(my_conn->fd, 
//                                      &my_conn->write_buffer[my_conn->write_sent], 
//                                      total_to_send, 0);
            
//             if (bytes_sent > 0) {
//                 my_conn->write_sent += bytes_sent;
//                 if (my_conn->write_sent == my_conn->write_buffer.size()) {
//                     // Done writing! Switch back to watching for inputs
//                     my_conn->write_buffer.clear();
//                     my_conn->write_sent = 0;
                    
//                     struct epoll_event ev = my_conn->event;
//                     ev.events = EPOLLIN | EPOLLET;
//                     epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my_conn->fd, &ev);
//                 }
//             }
//         }
//     }
// }