#ifndef THPL
#define THPL

#include <iostream>
#include "Blocking_Queue.h"
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
    std::atomic<bool>shutdown_flag = false;
    Connection_manager Conn_manager;
    int Max_events=1;
    void close_pool();
    // int N=0;
    int dispatcher_epoll=-1;

public:
    Thread_Pool(int epl_fd,int Max_events=1,int n_clients) : dispatcher_epoll(epl_fd),Max_events(Max_events)
                ,all_connections(n_clients+50,nullptr)
    {
        if(-1==epl_fd){
            std::cerr<<"[ERROR]:: Invalid dispacther fd. Thread_pool can't be initiated."<<std::endl;
            std::cout<<"[INFO]:: Use create_pool(Num,Epoll_fd) member instead to initiate Thread_pool"<<std::endl;
        }   
        std::cout << " [INFO]:: Thread_Pool initiated." << std::endl;
    }
    // define other ctorss.....
    ~Thread_Pool()
    {
        shutdown();
    }

    
    void create_pool(int epl_fd);
    void create_pool(int N, int epl_fd);
    void shutdown();
    void thread_func(int epl_fd,int Max_events);
};

#endif