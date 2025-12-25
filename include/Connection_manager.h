#ifndef CONN_MNGR_H
#define CONN_NMGR_H

#include"Connection.h"
#include"common_header.h"


class Connection_manager{
    private:
        // std::unordered_map<int ,std::shared_ptr<Connection>>live_connection;
        std::atomic<bool>closing_flag=false;
    public:
        void accept_all(int sock_fd,int epoll_fd,struct epoll_event &event,std::unordered_map<int ,std::shared_ptr<Connection>>&live_connection);
        void remove_all_connection();
        void close_connection(int fd,std::unordered_map<int ,std::shared_ptr<Connection>>&live_connection);

        ~Connection_manager(){
            if(!closing_flag){
                closing_flag=true;
                // remove_all_connection();
            }
        }
};

#endif