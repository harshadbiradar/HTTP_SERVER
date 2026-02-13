#ifndef CONN_MNGR_H
#define CONN_NMGR_H

#include"Connection.h"
#include"common_header.h"


class Connection_manager{
    private:
        // std::unordered_map<int ,std::shared_ptr<Connection>>live_connection;
        std::atomic<bool>closing_flag=false;
    public:
        void accept_all(int sock_fd,int epoll_fd,struct epoll_event &event);
        void remove_all_connection(int epoll_fd,std::vector<Connection*> &all_connections);
        void close_connection(int fd,int epoll_fd,std::vector<Connection*> &all_connections);
        void handle_client_fd(int epoll_fd,struct epoll_event &event,std::vector<Connection*> &all_connections);
        void try_write(Connection* Conn, int epoll_fd, std::vector<Connection*> &all_connections);

        ~Connection_manager(){
            if(!closing_flag){
                closing_flag=true;
                // remove_all_connection();
            }
        }
};

#endif