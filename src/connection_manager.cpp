#include"../include/Connection_manager.h"

void Connection_manager::accept_all(int sock_fd,int epoll_fd,struct epoll_event &event,
                std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections)
{
    int client_fd;
    sockaddr_in serverAddress;
    socklen_t len_sock=sizeof(serverAddress);
    int retcode;
    while (true)
    {
        std::cout << "In accept" << std::endl;
        client_fd = accept(sock_fd, (struct sockaddr *)&serverAddress, &len_sock);
        std::cout << "Client fd is " << client_fd << std::endl;
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
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
        if (retcode == -1)
        {
            std::cerr << "[ERROR]: Error modifying epoll_fd in epoll_ctl."<<strerror(errno) << std::endl;
            // terminate();
        }
        Connection conn(client_fd, event);
        Live_connections.try_emplace(client_fd, std::make_shared<Connection>(std::move(conn)));
    }
}


void Connection_manager::close_connection(int fd,int epoll_fd,std::unordered_map<int,std::shared_ptr<Connection>>&live_connections){
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