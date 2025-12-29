#include"../include/Thread_pool.h"

void Thread_Pool::thread_func(int index)
{
    while (true)
    {
        auto task = queue_pool[index].pop();
        if (!task) {
            // Empty task means shutdown
            // //std::cout<<" [DEBUG]:: exiting out from one of the thread.."<<std::endl;
            break;
        }
        task();
    }
}

// template <size_t N>
void Thread_Pool::create_pool(int N,int queue_size)
{   this->N=N;
    // BQ.set_size(queue_size);
    // //std::cout << " [INFO]:: Creating " << N << " Threads" << std::endl;
    
    for (int i = 0; i < N; ++i)
    {
        Blocking_Queue<std::function<void()>> BQ;
        BQ.set_size(queue_size/N);
        queue_pool.emplace_back(std::move(BQ));
    }
    for (int i = 0; i < N; ++i)
    {
        pool.emplace_back(&Thread_Pool::thread_func, this,i);
    }
}
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
void Thread_Pool::submit(int index,std::function<void()> task)
{
    queue_pool[index].push(std::move(task));
}

// template <size_t N>
void Thread_Pool::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(m);
        if (shutdown_flag)
            return;
        shutdown_flag = true;
        // BQ.shutdown();
        for(int i=0;i<N;i++)
            queue_pool[i].shutdown();

    }
    close_pool();
}