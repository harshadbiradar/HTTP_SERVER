#ifndef SERVER_H
#define SERVER_H
#include"common_header.h"
#include"config.h"
#include"Connection_manager.h"
#include"Thread_pool.h"

class Server{
    private:
        int epoll_fd = -1;
        int server_socket = -1;
        int notify_fd=-1;
        struct epoll_event event, notify_event;
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
        Thread_Pool Worker_thread;
        std::unordered_map<int ,std::shared_ptr<Connection>>live_connections;
        Connection_manager conn_man;
        Blocking_Queue<std::pair<int, std::string>> Writable_queue;
        public:
            Server(Config config){
                config.get_config(port, n_clients, max_events, n_workers, queue_size, buff_len);
                events.reserve(max_events);
                // Worker_thread.create_pool(n_workers);
                // Writable_queue.set_size(queue_size);
                setup_server();
            }
            void setup_server();
            void start();
            void stop();
            void terminate();
            void handle_notify_fd(int notify_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections);
            void handle_client_fd(struct epoll_event &event,std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections);
            void Req_handler(int fd, std::string &request);
};

#endif