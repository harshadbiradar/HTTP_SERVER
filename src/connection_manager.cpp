#include"../include/Connection_manager.h"



void Connection_manager::close_connection(int fd,int epoll_fd,std::unordered_map<int,std::shared_ptr<Connection>>&live_connections){
    // LOG_DEBUG("Closing connection: fd=" << fd << " | Remaining live: " << live_connections.size() - 1);
    int retcode = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    if (retcode == -1)
    {
        std::cerr << "[ERROR]: Error modifying fd in epoll_ctl." << std::endl;
        // terminate();
    }
    close(fd);
    live_connections.erase(fd);
}

void Connection_manager::remove_all_connection(int epoll_fd,std::unordered_map<int,std::shared_ptr<Connection>>&live_connection){
    for(auto it=live_connection.begin();it!=live_connection.end();){
        close_connection(it->second->fd,epoll_fd,live_connection);
        ++it;
    }
}



void Connection_manager::handle_client_fd(struct epoll_event &event,
                             Connection* Conn)
{

    if (event.events & EPOLLIN)
    {
        // Loop until EAGAIN (Required for Edge-Triggered mode)
        while (true)
        {   
            //LOG_DEBUG("In Read till EAGAIN loop...");
            // Calculate remaining space in the pre-allocated read_buffer
            int space_left = Conn->read_buffer.size() - Conn->read_idx;
            
            // If buffer is full, we must either handle the error or expand
            if (space_left <= 0) {
                // For high performance, we avoid reallocation, but for robustness:
                Conn->read_buffer.resize(Conn->read_buffer.size() * 2);
                space_left = Conn->read_buffer.size() - Conn->read_idx;
            }

            // 3. Raw recv into the specific offset of the vector
            ssize_t bytes_read = recv(Conn->fd, 
                                     &Conn->read_buffer[Conn->read_idx], 
                                     space_left, 0);

            if (bytes_read > 0)
            {   
                //LOG_DEBUG("Bytes recieved... "<<bytes_read);
                Conn->read_idx += bytes_read;
                // std::string_view current_data(&Conn->read_buffer[0], Conn->read_idx);
                // if (current_data.find('\n') != std::string_view::npos) {
                //     Req_handler(Conn->fd, Conn);
                // }
                while (true) { // Loop to process multiple requests in one buffer
                    std::string_view current_data(&Conn->read_buffer[0], Conn->read_idx);
                    auto pos = current_data.find('\n');
                    
                    if (pos != std::string_view::npos) {
                        // Req_handler(Conn->fd, Conn); // Logic below
                        
                        // SHIFT the leftover data to the front
                        size_t processed_len = pos + 1;
                        if (Conn->read_idx > processed_len) {
                            memmove(&Conn->read_buffer[0], 
                                    &Conn->read_buffer[processed_len], 
                                    Conn->read_idx - processed_len);
                            Conn->read_idx -= processed_len;
                        } else {
                            Conn->read_idx = 0;
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
                    conn_man.close_connection(Conn->fd, epoll_fd, live_connections);
                    return;
                }
            }
            else // bytes_read == 0 (Client closed connection)
            {
                //LOG_DEBUG("Bytes recieved... "<<bytes_read);
                conn_man.close_connection(Conn->fd, epoll_fd, live_connections);
                return;
            }
        }
    }
    else if (event.events & EPOLLOUT)
    {
        // 5. Optimized Write Path
        // Send from the write_buffer starting at write_sent offset
        size_t total_to_send = Conn->write_buffer.size() - Conn->write_sent;
        if (total_to_send > 0) {
            ssize_t bytes_sent = send(Conn->fd, 
                                     &Conn->write_buffer[Conn->write_sent], 
                                     total_to_send, 0);
            
            if (bytes_sent > 0) {
                Conn->write_sent += bytes_sent;
                if (Conn->write_sent == Conn->write_buffer.size()) {
                    // Done writing! Switch back to watching for inputs
                    Conn->write_buffer.clear();
                    Conn->write_sent = 0;
                    
                    struct epoll_event ev = Conn->event;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, Conn->fd, &ev);
                }
            }
        }
    }
}