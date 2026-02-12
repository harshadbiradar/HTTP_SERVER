#ifndef THPL
#define THPL

#include <iostream>
#include <thread>
#include <vector>
#include <functional>
#include <sys/epoll.h>
#include <Connection_manager.h>


// template <size_t N>
class Thread_Pool
{
private:
    std::vector<std::thread> pool;
    std::vector<Connection *>all_connections;
    std::atomic<bool>shutdown_flag {false};
    Connection_manager Conn_manager;
    int Max_evnt_p_thd;
    void close_pool();
    // int N=0;
    int dispatcher_epoll=-1;

public:
    Thread_Pool(int Max_evnt_p_thd,int n_clients) : Max_evnt_p_thd(Max_evnt_p_thd)
                ,all_connections(n_clients+50,nullptr)
    {  
        std::cout << " [INFO]:: Thread_Pool initiated." << std::endl;
    }
    // define other ctorss.....
    ~Thread_Pool()
    {
        shutdown();
    }

    
    // void create_pool(int epl_fd);
    void create_pool(int N, int epl_fd);
    void shutdown();
    void thread_func(int epl_fd,int Max_events);
};

#endif