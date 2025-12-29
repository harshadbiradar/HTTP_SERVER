#ifndef BLCK_QUE
#define BLCK_QUE

#include"common_header.h"
#include <queue>
#include <mutex>
#include <condition_variable>


template <typename T>
class Blocking_Queue
{
private:
    std::queue<T> queue;
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> shutdown_flag = false;
    std::atomic<int> max_size;
    bool is_full();
    int size();

public:
    Blocking_Queue()
    {   
        //setting this as default, incase user forgot to set_size
        max_size = 100;
    }

    Blocking_Queue(const Blocking_Queue &other) = delete;
    Blocking_Queue &operator=(const Blocking_Queue &other) = delete;

    // Blocking_Queue(Blocking_Queue &&other) noexcept:max_size(other.max_size.load()),queue(std::move(other.queue)),m(),cv()
    // {
    //     // max_size = other.max_size;
    //     other.max_size = 0;
    //     // while (!other.queue.empty())
    //     // {
    //     //     queue.emplace(std::move(other.queue.front()));
    //     //     other.queue.pop();
    //     // }
    // }

    Blocking_Queue(Blocking_Queue&& other) noexcept
                : queue(std::move(other.queue)),
                max_size(other.max_size.load(std::memory_order_relaxed)),
                shutdown_flag(other.shutdown_flag.load(std::memory_order_relaxed)),
                m(),      // mutex is not movable — default-construct
                cv()
    {
        // Leave 'other' in a valid (usually empty) state
        other.max_size.store(0, std::memory_order_relaxed);
        other.shutdown_flag.store(false, std::memory_order_relaxed);
        // queue is already moved-from → valid but unspecified (usually empty)
    }


    // Blocking_Queue &operator=(Blocking_Queue &&other) noexcept
    // {
    //     // move assign ctor
    //     if (this != &other)
    //     {
    //         max_size = other.max_size.load();
    //         other.max_size.store(0);
    //         // while (!queue.empty())
    //         // {
    //         //     queue.pop();
    //         // }
    //         // while (!other.queue.empty())
    //         // {
    //         //     queue.emplace(std::move(other.queue.front()));
    //         //     other.queue.pop();
    //         // }
    //     }
    //     return *this;
    // }


    // Move assignment operator
    Blocking_Queue& operator=(Blocking_Queue&& other) noexcept
    {
        if (this != &other)
        {
            // Move the queue (this replaces our current content)
            queue = std::move(other.queue);

            // Properly move the atomics
            max_size.store(other.max_size.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
            shutdown_flag.store(other.shutdown_flag.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);

            // Leave 'other' in a valid state
            other.max_size.store(0, std::memory_order_relaxed);
            other.shutdown_flag.store(false, std::memory_order_relaxed);
        }
        return *this;
    }

    template <typename K>
    void push(K &&data);
    T pop();
    T try_pop();
    int safe_size();
    void shutdown();
    void set_size(int n){
        max_size=n;
    }
    // bool is_empty();
};

template <typename T>
template <typename K>
void Blocking_Queue<T>::push(K &&data)
{
    std::unique_lock<std::mutex> lock(m); // it gets lock safely, below is CS
    // if(shutdown)return;
    while (is_full() && !shutdown_flag)
    {
        cv.wait(lock);
    }
    if (shutdown_flag)
    {
        //std::cout << " [WARNING]:: Push is called while SHUTDOWN is in progress, operation not allowed" << std::endl;
        return;
    }

    // cv.wait(lock,[this]{return !is_full();});
    /*
    here I will enter in never ending loop, which ends when condition is flase
    the logic is since this particular function is called in thread,the opreations
    here will be threads operation, hence calling wait, will make this thread sleep
    and release locks and let other thread do work.
    */

    // queue is not full, so push.
    queue.emplace(std::forward<K>(data));
    //std::cout << " [INFO]:: Pushed data: "<< std::endl;
    cv.notify_one();
}

template <typename T>
T Blocking_Queue<T>::pop()
{
    // handle state here
    // T data = T(); // Default-constructed value
    std::unique_lock<std::mutex> lock(m);
    while (queue.empty() && !shutdown_flag)
    {
        cv.wait(lock);
    }
    if (shutdown_flag && queue.empty()){
        // //std::cout<<" [DEBUG]:: IN BQ POP in shutting down condition... "<<std::endl;
        return T{};
    }
    /*so if shutdown is set, we break from this loop,
        giving thread chance to terminate from user side.*/
    // cv.wait(lock,[this]{return !queue.empty();});// logic explained in push, same here.
    T data = queue.front();
    //std::cout << " [INFO]:: Data poped: " << std::endl;
    queue.pop();
    cv.notify_one();
    return data;
}


template <typename T>
T Blocking_Queue<T>::try_pop()
{
    //this is a wait free pop,
    std::unique_lock<std::mutex> lock(m);
    if ( queue.empty()){
        // //std::cout<<" [DEBUG]:: IN BQ POP in shutting down condition... "<<std::endl;
        return T{};
    }
    T data = queue.front();
    //std::cout << " [INFO]:: Data poped: " << std::endl;
    queue.pop();
    cv.notify_one();
    return data;
}

template <typename T>
int Blocking_Queue<T>::size()
{
    return queue.size();
}
template <typename T>
int Blocking_Queue<T>::safe_size()
{
    std::unique_lock<std::mutex> lock(m);
    return queue.size();
}

template <typename T>
bool Blocking_Queue<T>::is_full()
{
    return queue.size() == max_size;
}

template <typename T>
void Blocking_Queue<T>::shutdown()
{
    std::unique_lock<std::mutex> lock(m);
    //std::cout << " [INFO]:: SHUTDOWN for Blocking_Queue is being initiallized." << std::endl;
    shutdown_flag = true;
    cv.notify_all();
    //std::cout << " [INFO]:: SHUTDOWN initiated." << std::endl;
}

#endif

/*
Remember-- the functions or the operations being called on the Critical section (queue here) must be
thread safe might need locking mechanism, always verify and cross check

Like here , still the Move ctor and Move assign ctor are not thread safe. so be carefull.
*/