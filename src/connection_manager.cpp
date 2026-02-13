#include"../include/Connection_manager.h"


void Connection_manager::accept_all(int sock_fd,int epoll_fd,struct epoll_event &event)
{
    int client_fd;
    sockaddr_in serverAddress;
    socklen_t len_sock=sizeof(serverAddress);
    int retcode;
    while (true)
    {
        // std::cout << "In accept" << std::endl;
        client_fd = accept(sock_fd, (struct sockaddr *)&serverAddress, &len_sock);
        // std::cout << "Client fd is " << client_fd << std::endl;
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
            {
                std::cerr << "[ERROR]: Error in accept." << strerror(errno) << std::endl;
                // terminate();
                // also handle epoll_close,server_fd close
            }
        }
        event.events = EPOLLIN|EPOLLET;
        event.data.fd = client_fd;
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
        if (retcode == -1)
        {
            std::cerr << "[ERROR]: Error modifying dispacth_epoll in epoll_ctl."<<strerror(errno) << std::endl;
            // terminate();
        }
        // Connection conn(client_fd, event);
    }
}





void Connection_manager::close_connection(int fd,int epoll_fd,std::vector<Connection*> &all_connections){
    // LOG_DEBUG("Closing connection: fd=" << fd << " | Remaining live: " << live_connections.size() - 1);
    int retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    if (retcode == -1)
    {
        std::cerr << "[ERROR]: Error modifying fd in epoll_ctl." << std::endl;
        // terminate();
    }
    if(all_connections[fd]!=nullptr){
        // std::cout<<"Deleting the connections//"<<std::endl;
        delete all_connections[fd];
        all_connections[fd]=nullptr;
    }
    close(fd);
}

void Connection_manager::remove_all_connection(int epoll_fd,std::vector<Connection*> &all_connections){
    for(auto &it:all_connections){
        if(it!=nullptr)
            close_connection(it->fd,epoll_fd,all_connections);
    }
}



// void Connection_manager::handle_client_fd(int epoll_fd,struct epoll_event &event,
//                              std::vector<Connection*> &all_connections)
// {
//     Connection* Conn=all_connections[event.data.fd];

//     if (event.events & EPOLLIN)
//     {
//         // Loop until EAGAIN (Required for Edge-Triggered mode)
//         while (true)
//         {   
//             //LOG_DEBUG("In Read till EAGAIN loop...");
//             // Calculate remaining space in the pre-allocated read_buffer
//             int space_left = Conn->read_buffer.size() - Conn->read_idx;
            
//             // If buffer is full, we must either handle the error or expand
//             if (space_left <= 0) {
//                 // For high performance, we avoid reallocation, but for robustness:
//                 Conn->read_buffer.resize(Conn->read_buffer.size() * 2);
//                 space_left = Conn->read_buffer.size() - Conn->read_idx;
//             }

//             // 3. Raw recv into the specific offset of the vector
//             ssize_t bytes_read = recv(Conn->fd, 
//                                      &Conn->read_buffer[Conn->read_idx], 
//                                      space_left, 0);

//             if (bytes_read > 0)
//             {   
//                 //LOG_DEBUG("Bytes recieved... "<<bytes_read);
//                 Conn->read_idx += bytes_read;
//                 // std::string_view current_data(&Conn->read_buffer[0], Conn->read_idx);
//                 // if (current_data.find('\n') != std::string_view::npos) {
//                 //     Req_handler(Conn->fd, Conn);
//                 // }
//                 while (true) { // Loop to process multiple requests in one buffer
//                     std::string_view current_data(&Conn->read_buffer[0], Conn->read_idx);
//                     auto pos = current_data.find('\n');
                    
//                     if (pos != std::string_view::npos) {
//                         // Req_handler(Conn->fd, Conn); // Logic below
//                         Conn->write_buffer.insert(Conn->write_buffer.end(),
//                              &Conn->read_buffer[0],
//                              &Conn->read_buffer[pos + 1]);
                        
//                         // SHIFT the leftover data to the front
//                         size_t processed_len = pos + 1;
//                         if (Conn->read_idx > processed_len) {
//                             memmove(&Conn->read_buffer[0], 
//                                     &Conn->read_buffer[processed_len], 
//                                     Conn->read_idx - processed_len);
//                             Conn->read_idx -= processed_len;
//                         } else {
//                             Conn->read_idx = 0;
//                             break; // No more data to process
//                         }

//                     } else {
//                         break; // No complete request found yet
//                     }
//                 }

//             }
//             else if (bytes_read == -1)
//             {
//                 // std::cout<<"In bytes_read==-1 bolck..."<<std::endl;
//                 //LOG_DEBUG("Bytes recieved... "<<bytes_read);
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // Kernel buffer is empty, stop reading, and initiate write..
                    
//                     event.events = EPOLLOUT|EPOLLET;
//                     int retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &event);
//                     if (retcode == -1)
//                     {
//                         std::cerr << "[ERROR]: Error modifying dispacth_epoll in epoll_ctl."<<strerror(errno) << std::endl;
//                         // terminate();
//                     }
//                     // std::cout<<"In breakkk bolck..."<<std::endl;
//                     break;
//                 } else if (errno == EINTR) {
//                     // Interrupted by signal, try again immediately
//                     continue;
//                 } else {
//                     // Actual error
//                     close_connection(Conn->fd, epoll_fd, all_connections);
//                     return;
//                 }
//             }
//             else // bytes_read == 0 (Client closed connection)
//             {
//                 //LOG_DEBUG("Bytes recieved... "<<bytes_read);
//                 close_connection(Conn->fd, epoll_fd, all_connections);
//                 return;
//             }
//         }
//     }
//     else if (event.events & EPOLLOUT)
//     {
//         {

//             // std::cout<<"In write bolck..."<<std::endl;

//             // Optimized Write Path
//             // Send from the write_buffer starting at write_sent offset
//             // Conn->write_buffer.push_back('H');
//             // Conn->write_buffer.push_back('e');
//             // Conn->write_buffer.push_back('l');
//             // Conn->write_buffer.push_back('l');
//             // Conn->write_buffer.push_back('o');
//             // Conn->write_buffer.push_back('\n');
//             size_t total_to_send = Conn->write_buffer.size() - Conn->write_sent;
//             if (total_to_send > 0) {
//                 ssize_t bytes_sent = send(Conn->fd, 
//                                         &Conn->write_buffer[Conn->write_sent], 
//                                         total_to_send, 0);
                
//                 if (bytes_sent > 0) {
//                     Conn->write_sent += bytes_sent;
//                     if (Conn->write_sent == Conn->write_buffer.size()) {
//                         // Done writing! Switch back to watching for inputs
//                         Conn->write_buffer.clear();
//                         Conn->write_sent = 0;
                        
//                         struct epoll_event ev = Conn->event;
//                         ev.events = EPOLLIN | EPOLLET;
//                         epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &ev);
//                     }
//                 }
//             }

//         }
//     }
// }

void Connection_manager::handle_client_fd(int epoll_fd, struct epoll_event &event,
                             std::vector<Connection*> &all_connections)
{

    Connection* Conn = all_connections[event.data.fd];

    if (event.events & EPOLLIN)
    {
        // Read until EAGAIN
        while (true)
        {   
            int space_left = Conn->read_buffer.size() - Conn->read_idx;
            
            if (space_left <= 0) {
                Conn->read_buffer.resize(Conn->read_buffer.size() * 2);
                space_left = Conn->read_buffer.size() - Conn->read_idx;
            }

            ssize_t bytes_read = recv(Conn->fd, 
                                     &Conn->read_buffer[Conn->read_idx], 
                                     space_left, 0);

            if (bytes_read > 0)
            {   
                Conn->read_idx += bytes_read;
                
                // Process all complete requests in buffer
                while (true) {
                    std::string_view current_data(&Conn->read_buffer[0], Conn->read_idx);
                    auto pos = current_data.find('\n');
                    
                    if (pos != std::string_view::npos) {
                        // Generate response
                        // Conn->write_buffer.insert(Conn->write_buffer.end(),
                        //      &Conn->read_buffer[0],
                        //      &Conn->read_buffer[pos + 1]);
                        Conn->write_buffer.push_back('H');
                        Conn->write_buffer.push_back('e');
                        Conn->write_buffer.push_back('l');
                        Conn->write_buffer.push_back('l');
                        Conn->write_buffer.push_back('o');
                        Conn->write_buffer.push_back('\n');
                        
                        // Shift buffer
                        size_t processed_len = pos + 1;
                        if (Conn->read_idx > processed_len) {
                            memmove(&Conn->read_buffer[0], 
                                    &Conn->read_buffer[processed_len], 
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
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Done reading - now try to write immediately!
                    break;  // Exit read loop
                } else if (errno == EINTR) {
                    continue;
                } else {
                    close_connection(Conn->fd, epoll_fd, all_connections);
                    return;
                }
            }
            else // bytes_read == 0
            {
                close_connection(Conn->fd, epoll_fd, all_connections);
                return;
            }
        }
        
        // CRITICAL: Try to write immediately after reading!
        if (!Conn->write_buffer.empty()) {
            try_write(Conn, epoll_fd, all_connections);
        }
    }
    if (event.events & EPOLLOUT)
    {
        try_write(Conn, epoll_fd, all_connections);
    }
}

// Separate write function - called from both EPOLLIN and EPOLLOUT
void Connection_manager::try_write(Connection* Conn, int epoll_fd, 
                                   std::vector<Connection*> &all_connections)
{
    // Write until EAGAIN or all data sent
    while (Conn->write_sent < Conn->write_buffer.size())
    {
        size_t remaining = Conn->write_buffer.size() - Conn->write_sent;
        ssize_t bytes_sent = send(Conn->fd, 
                                 &Conn->write_buffer[Conn->write_sent], 
                                 remaining, 0);
        
        if (bytes_sent > 0) {
            Conn->write_sent += bytes_sent;
        }
        else if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Can't write more - need to wait for EPOLLOUT
                struct epoll_event ev = Conn->event;
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // Watch BOTH!
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &ev);
                return;
            }
            else if (errno == EINTR) {
                continue;  // Try again
            }
            else {
                // Error - close connection
                close_connection(Conn->fd, epoll_fd, all_connections);
                return;
            }
        }
    }
    
    // All data written successfully!
    Conn->write_buffer.clear();
    Conn->write_sent = 0;
    
    // Back to just watching for reads
    struct epoll_event ev = Conn->event;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &ev);
}