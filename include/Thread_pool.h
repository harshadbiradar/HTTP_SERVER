#ifndef THPL
#define THPL

#include "Blocking_Queue.h"
#include <thread>
#include <vector>
#include <functional>

// template <size_t N>
class Thread_Pool
{
private:
    std::mutex m;
    std::vector<std::thread> pool;
    std::vector<Blocking_Queue<std::function<void()>>> queue_pool;
    // Blocking_Queue<std::function<void()>> BQ;
    bool shutdown_flag = false;
    void close_pool();
    int N=0;

public:
    // Thread_Pool() : BQ(100)
    // {
    //     std::cout << " [INFO]:: Thread_Pool initiated." << std::endl;
    // }
    // define other ctorss.....
    ~Thread_Pool()
    {
        shutdown();
    }

    void submit(int index,std::function<void()>);
    void create_pool(int N,int queue_size);
    void shutdown();
    void thread_func(int index);
};

#endif