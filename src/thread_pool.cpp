#include"../include/Thread_pool.h"

void Thread_Pool::thread_func(int epoll_fd,int max_events)
{
    // epoll_wait here....
    std::vector<epoll_event>events;
    struct epoll_event event;
    events.reserve(max_events);
    Connection *Conn;
    int n=0;
    while(!shutdown_flag||n){
        // std::cout<<"Waiting at epoll_wait"<<std::endl;
        n=epoll_wait(epoll_fd,events.data(),Max_evnt_p_thd,-1);
        n=(max_events>n)?n:max_events;//So wont endup handling, the extra loop(causing UB).
        for(int i=0;i<n;i++){
            //handle the fd...
            // LOG_DEBUG("Going to client_handle for "<<events[i].data.fd);
            if(all_connections[events[i].data.fd]==nullptr){
                Conn=new Connection(events[i].data.fd,events[i]);
                all_connections[events[i].data.fd]=Conn;
                // std::cout<<"allocated mate"<<std::endl;
            }
            Conn_manager.handle_client_fd(epoll_fd,events[i], all_connections);
        }
    }

}

// template <size_t N>
void Thread_Pool::create_pool(int N,int epl_fd)
{   
    if(-1==epl_fd){
        std::cerr<<"[ERROR]:: Invalid dispacther fd. Thread_pool can't be initiated."<<std::endl;
    } 
    this->dispatcher_epoll=epl_fd;
    for (int i = 0; i < N; ++i)
    {
        pool.emplace_back(&Thread_Pool::thread_func, this,epl_fd,Max_evnt_p_thd);
    }
}

// void Thread_Pool::create_pool(int N){
//     for (int i = 0; i < N; ++i)
//     {
//         pool.emplace_back(&Thread_Pool::thread_func, this,dispatcher_epoll);
//     }
// }
// template <size_t N>
void Thread_Pool::close_pool()
{
    //std::cout << " [INFO]:: Terminating " << N << " Threads" << std::endl;
    for (auto &t : pool)
    {
        if(t.joinable())t.join();
        // //std::cout<<" Hello there"<<std::endl;
    }
}

// template <size_t N>


// template <size_t N>
void Thread_Pool::shutdown()
{
    {
        if (shutdown_flag)
            return;
        shutdown_flag = true;

    }
    close_pool();
}



