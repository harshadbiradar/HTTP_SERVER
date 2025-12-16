#ifndef THPL
#define THPL

#include "Blocking_Queue.h"
#include <thread>
#include <vector>
#include <functional>

template <size_t N>
class Thread_Pool
{
private:
    std::mutex m;
    std::vector<std::thread> pool;
    Blocking_Queue<std::function<void()>> BQ;
    bool shutdown_flag = false;
    void close_pool();

public:
    Thread_Pool() : BQ(100)
    {
        std::cout << " [INFO]:: Thread_Pool initiated." << std::endl;
    }
    // define other ctorss.....
    ~Thread_Pool()
    {
        shutdown();
    }

    void submit(std::function<void()>);
    void create_pool();
    void shutdown();
    void thread_func();
};

template <size_t N>
void Thread_Pool<N>::thread_func()
{
    while (true)
    {
        auto task = BQ.pop();
        if (!task) {
            // Empty task means shutdown
            // std::cout<<" [DEBUG]:: exiting out from one of the thread.."<<std::endl;
            break;
        }
        task();
    }
}

template <size_t N>
void Thread_Pool<N>::create_pool()
{
    std::cout << " [INFO]:: Creating " << N << " Threads" << std::endl;
    for (int i = 0; i < N; i++)
    {

        pool.emplace_back(&Thread_Pool::thread_func, this);
    }
}
template <size_t N>
void Thread_Pool<N>::close_pool()
{
    std::cout << " [INFO]:: Terminating " << N << " Threads" << std::endl;
    for (auto &t : pool)
    {
        if(t.joinable())t.join();
        // std::cout<<" Hello there"<<std::endl;
    }
}

template <size_t N>
void Thread_Pool<N>::submit(std::function<void()> task)
{
    BQ.push(std::move(task));
}

template <size_t N>
void Thread_Pool<N>::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(m);
        if (shutdown_flag)
            return;
        shutdown_flag = true;
        BQ.shutdown();
    }
    close_pool();
}

#endif