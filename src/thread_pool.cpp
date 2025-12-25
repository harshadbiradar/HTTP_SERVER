#include"../include/Thread_pool.h"

void Thread_Pool::thread_func()
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

// template <size_t N>
void Thread_Pool::create_pool(int N)
{   
    BQ.set_size(N*100);
    std::cout << " [INFO]:: Creating " << N << " Threads" << std::endl;
    for (int i = 0; i < N; i++)
    {

        pool.emplace_back(&Thread_Pool::thread_func, this);
    }
}
// template <size_t N>
void Thread_Pool::close_pool()
{
    std::cout << " [INFO]:: Terminating " << N << " Threads" << std::endl;
    for (auto &t : pool)
    {
        if(t.joinable())t.join();
        // std::cout<<" Hello there"<<std::endl;
    }
}

// template <size_t N>
void Thread_Pool::submit(std::function<void()> task)
{
    BQ.push(std::move(task));
}

// template <size_t N>
void Thread_Pool::shutdown()
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