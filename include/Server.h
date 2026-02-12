#ifndef SERVER_H
#define SERVER_H
#include"common_header.h"
#include"config.h"
// #include"Connection_manager.h"
#include"Thread_pool.h"

class Server{
    private:
        int recv_epoll = -1;
        int dispacth_epoll=-1;
        int server_socket = -1;
        struct epoll_event event;
        std::vector<epoll_event>events;
        int port;
    private:
        int max_events;
        int n_clients;
        int n_workers;
        int queue_size;
        int buff_len;
    private:
        std::atomic<bool> running=false;
        Thread_Pool &Workers;
        std::unordered_map<int ,std::shared_ptr<Connection>>live_connections;
        Connection_manager conn_man;
        // Blocking_Queue<std::pair<int, std::string>> Writable_queue;
    public:
        Server(Config config,Thread_Pool &Workers):Workers(Workers){
            config.get_config(port, n_clients, max_events, n_workers, queue_size, buff_len);
            events.reserve(max_events);
            
            // Writable_queue.set_size(queue_size);
            setup_server();
        }
        void setup_server();
        void start();
        void stop();
        void terminate();
        void handle_notify_fd(int notify_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections);
        void handle_client_fd(struct epoll_event &event,std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections);
        // void Req_handler(int fd, std::string &&request);
        void Req_handler(int fd, std::shared_ptr<Connection> my_conn);

};

#endif