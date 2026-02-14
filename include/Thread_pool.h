#ifndef THPL
#define THPL

#include <iostream>
#include <thread>
#include <vector>
#include <functional>
#include <sys/epoll.h>
#include<config.h>
#include <Connection_manager.h>


// template <size_t N>
class Thread_Pool
{
private:
    int max_events;
    int n_clients;
    int n_workers;
    int buff_len;
    int Max_evnt_p_thd;
    int port;
    // int queue_size;
    // int N=0;
    // int dispatcher_epoll=-1;
    std::vector<std::thread> pool;
    std::atomic<bool>shutdown_flag {false};
    Connection_manager Conn_manager;
    // std::vector<Connection *>all_connections;
    void close_pool();

public:
    Thread_Pool(Config config) 
    {  
        std::cout << " [INFO]:: Thread_Pool initiated." << std::endl;
        config.get_config(port, n_clients, max_events, n_workers, buff_len,Max_evnt_p_thd);
    }
    // define other ctorss.....
    ~Thread_Pool()
    {
        shutdown();
    }

    
    // void create_pool(int epl_fd);
    void create_pool(int N);
    void shutdown();
    void thread_func();
    void setup_server(int &server_socket,int &epoll_fd,struct epoll_event &event);
    void terminate(int &server_socket,int &epoll_fd);
};

#endif