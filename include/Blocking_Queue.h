// #ifndef BLCK_QUE
// #define BLCK_QUE

// #include"common_header.h"
// #include <queue>
// #include <mutex>
// #include <condition_variable>


// template <typename T>
// class Blocking_Queue
// {
// private:
//     std::queue<T> queue;
//     std::mutex m;
//     std::condition_variable cv;
//     std::atomic<bool> shutdown_flag = false;
//     std::atomic<int> max_size;
//     bool is_full();
//     int size();

// public:
//     Blocking_Queue()
//     {   
//         //setting this as default, incase user forgot to set_size
//         max_size = 100;
//     }

//     Blocking_Queue(const Blocking_Queue &other) = delete;
//     Blocking_Queue &operator=(const Blocking_Queue &other) = delete;

//     // Blocking_Queue(Blocking_Queue &&other) noexcept:max_size(other.max_size.load()),queue(std::move(other.queue)),m(),cv()
//     // {
//     //     // max_size = other.max_size;
//     //     other.max_size = 0;
//     //     // while (!other.queue.empty())
//     //     // {
//     //     //     queue.emplace(std::move(other.queue.front()));
//     //     //     other.queue.pop();
//     //     // }
//     // }

//     Blocking_Queue(Blocking_Queue&& other) noexcept
//                 : queue(std::move(other.queue)),
//                 max_size(other.max_size.load(std::memory_order_relaxed)),
//                 shutdown_flag(other.shutdown_flag.load(std::memory_order_relaxed)),
//                 m(),      // mutex is not movable — default-construct
//                 cv()
//     {
//         // Leave 'other' in a valid (usually empty) state
//         other.max_size.store(0, std::memory_order_relaxed);
//         other.shutdown_flag.store(false, std::memory_order_relaxed);
//         // queue is already moved-from → valid but unspecified (usually empty)
//     }


//     // Blocking_Queue &operator=(Blocking_Queue &&other) noexcept
//     // {
//     //     // move assign ctor
//     //     if (this != &other)
//     //     {
//     //         max_size = other.max_size.load();
//     //         other.max_size.store(0);
//     //         // while (!queue.empty())
//     //         // {
//     //         //     queue.pop();
//     //         // }
//     //         // while (!other.queue.empty())
//     //         // {
//     //         //     queue.emplace(std::move(other.queue.front()));
//     //         //     other.queue.pop();
//     //         // }
//     //     }
//     //     return *this;
//     // }


//     // Move assignment operator
//     Blocking_Queue& operator=(Blocking_Queue&& other) noexcept
//     {
//         if (this != &other)
//         {
//             // Move the queue (this replaces our current content)
//             queue = std::move(other.queue);

//             // Properly move the atomics
//             max_size.store(other.max_size.load(std::memory_order_relaxed),
//                         std::memory_order_relaxed);
//             shutdown_flag.store(other.shutdown_flag.load(std::memory_order_relaxed),
//                                 std::memory_order_relaxed);

//             // Leave 'other' in a valid state
//             other.max_size.store(0, std::memory_order_relaxed);
//             other.shutdown_flag.store(false, std::memory_order_relaxed);
//         }
//         return *this;
//     }

//     template <typename K>
//     void push(K &&data);
//     T pop();
//     T try_pop();
//     int safe_size();
//     void shutdown();
//     void set_size(int n){
//         max_size=n;
//     }
//     // bool is_empty();
// };

// template <typename T>
// template <typename K>
// void Blocking_Queue<T>::push(K &&data)
// {
//     std::unique_lock<std::mutex> lock(m); // it gets lock safely, below is CS
//     // if(shutdown)return;
//     while (is_full() && !shutdown_flag)
//     {
//         cv.wait(lock);
//     }
//     if (shutdown_flag)
//     {
//         //std::cout << " [WARNING]:: Push is called while SHUTDOWN is in progress, operation not allowed" << std::endl;
//         return;
//     }

//     // cv.wait(lock,[this]{return !is_full();});
//     /*
//     here I will enter in never ending loop, which ends when condition is flase
//     the logic is since this particular function is called in thread,the opreations
//     here will be threads operation, hence calling wait, will make this thread sleep
//     and release locks and let other thread do work.
//     */

//     // queue is not full, so push.
//     queue.emplace(std::forward<K>(data));
//     //std::cout << " [INFO]:: Pushed data: "<< std::endl;
//     cv.notify_one();
// }

// template <typename T>
// T Blocking_Queue<T>::pop()
// {
//     // handle state here
//     // T data = T(); // Default-constructed value
//     std::unique_lock<std::mutex> lock(m);
//     while (queue.empty() && !shutdown_flag)
//     {
//         cv.wait(lock);
//     }
//     if (shutdown_flag && queue.empty()){
//         // //std::cout<<" [DEBUG]:: IN BQ POP in shutting down condition... "<<std::endl;
//         return T{};
//     }
//     /*so if shutdown is set, we break from this loop,
//         giving thread chance to terminate from user side.*/
//     // cv.wait(lock,[this]{return !queue.empty();});// logic explained in push, same here.
//     T data = queue.front();
//     //std::cout << " [INFO]:: Data poped: " << std::endl;
//     queue.pop();
//     cv.notify_one();
//     return data;
// }


// template <typename T>
// T Blocking_Queue<T>::try_pop()
// {
//     //this is a wait free pop,
//     std::unique_lock<std::mutex> lock(m);
//     if ( queue.empty()){
//         // //std::cout<<" [DEBUG]:: IN BQ POP in shutting down condition... "<<std::endl;
//         return T{};
//     }
//     T data = queue.front();
//     //std::cout << " [INFO]:: Data poped: " << std::endl;
//     queue.pop();
//     cv.notify_one();
//     return data;
// }

// template <typename T>
// int Blocking_Queue<T>::size()
// {
//     return queue.size();
// }
// template <typename T>
// int Blocking_Queue<T>::safe_size()
// {
//     std::unique_lock<std::mutex> lock(m);
//     return queue.size();
// }

// template <typename T>
// bool Blocking_Queue<T>::is_full()
// {
//     return queue.size() == max_size;
// }

// template <typename T>
// void Blocking_Queue<T>::shutdown()
// {
//     std::unique_lock<std::mutex> lock(m);
//     //std::cout << " [INFO]:: SHUTDOWN for Blocking_Queue is being initiallized." << std::endl;
//     shutdown_flag = true;
//     cv.notify_all();
//     //std::cout << " [INFO]:: SHUTDOWN initiated." << std::endl;
// }

// #endif



// #ifndef BLCK_QUE
// #define BLCK_QUE

// #include "common_header.h"
// #include <queue>
// #include <mutex>
// #include <condition_variable>
// #include <atomic>

// template <typename T>
// class Blocking_Queue
// {
// private:
//     std::queue<T> queue;
//     std::mutex m;
//     std::condition_variable cv;
//     std::atomic<bool> shutdown_flag{false};
//     std::atomic<int> max_size;

//     bool is_full_internal() { return queue.size() >= max_size; }

// public:
//     Blocking_Queue() : max_size(100) {}

//     // Delete copy operations
//     Blocking_Queue(const Blocking_Queue &other) = delete;
//     Blocking_Queue &operator=(const Blocking_Queue &other) = delete;

//     // --- Move Constructor ---
//     Blocking_Queue(Blocking_Queue&& other) noexcept
//     {
//         // Lock the other queue to ensure we move it in a consistent state
//         std::lock_guard<std::mutex> lock(other.m);
        
//         this->queue = std::move(other.queue);
//         this->max_size.store(other.max_size.load());
//         this->shutdown_flag.store(other.shutdown_flag.load());
        
//         // Note: m and cv are default initialized for the new object.
//         // Sync primitives cannot be moved.
//     }

//     // --- Move Assignment ---
//     Blocking_Queue& operator=(Blocking_Queue&& other) noexcept
//     {
//         if (this != &other) {
//             // Lock both to prevent deadlocks or data corruption
//             std::scoped_lock lock(this->m, other.m);
            
//             this->queue = std::move(other.queue);
//             this->max_size.store(other.max_size.load());
//             this->shutdown_flag.store(other.shutdown_flag.load());
            
//             this->cv.notify_all(); // Wake any threads waiting on the old state
//         }
//         return *this;
//     }

//     void set_size(int size) {
//         max_size = size;
//     }

//     // Returns true if the queue was empty before this push (Optimization)
//     bool push(T&& data)
//     {
//         std::unique_lock<std::mutex> lock(m);
//         cv.wait(lock, [this]() { return queue.size() < max_size || shutdown_flag; });

//         if (shutdown_flag) return false;

//         bool was_empty = queue.empty();
//         queue.emplace(std::forward<T>(data));
        
//         lock.unlock();
//         cv.notify_one();
//         return was_empty;
//     }

//     T pop()
//     {
//         std::unique_lock<std::mutex> lock(m);
//         cv.wait(lock, [this]() { return !queue.empty() || shutdown_flag; });

//         if (shutdown_flag && queue.empty()) return T{};

//         T data = std::move(queue.front());
//         queue.pop();
        
//         lock.unlock();
//         cv.notify_one();
//         return data;
//     }

//     // Non-blocking pop for the Drain Loop
//     std::pair<bool, T> try_pop()
//     {
//         std::unique_lock<std::mutex> lock(m);
//         if (queue.empty()) return {false, T{}};

//         T data = std::move(queue.front());
//         queue.pop();
        
//         lock.unlock();
//         cv.notify_one();
//         return {true, std::move(data)};
//     }

//     void shutdown()
//     {
//         std::unique_lock<std::mutex> lock(m);
//         shutdown_flag = true;
//         cv.notify_all();
//     }
// };

// #endif



/*
Remember-- the functions or the operations being called on the Critical section (queue here) must be
thread safe might need locking mechanism, always verify and cross check

Like here , still the Move ctor and Move assign ctor are not thread safe. so be carefull.
*/







// #ifndef BLCK_QUE
// #define BLCK_QUE

// #include <queue>
// #include <mutex>
// #include <condition_variable>
// #include <atomic>
// #include <vector>

// template <typename T>
// class Blocking_Queue {
// private:
//     std::queue<T> queue;
//     std::mutex m;
//     std::condition_variable cv;
//     std::atomic<bool> shutdown_flag{false};
//     std::atomic<int> max_size = 1024;

// public:
//     Blocking_Queue() = default;

//     // Explicit Move Constructor: Mutexes cannot be moved!
//     Blocking_Queue(Blocking_Queue&& other) noexcept :m(),cv(){
//         // std::lock_guard<std::mutex> lock(other.m);
//         this->queue = std::move(other.queue);
//         // this->max_size = other.max_size;
//         this->max_size.store(other.max_size.load());
//         this->shutdown_flag.store(other.shutdown_flag.load());
//         // Mutex and CV are default initialized in 'this'
//     }

//     void set_size(int size) { max_size = size; }
//      // --- Move Assignment ---
//     Blocking_Queue& operator=(Blocking_Queue&& other) noexcept
//     {
//         if (this != &other) {
//             // Lock both to prevent deadlocks or data corruption
//             // std::scoped_lock lock(this->m, other.m);
            
//             this->queue = std::move(other.queue);
//             this->max_size.store(other.max_size.load());
//             this->shutdown_flag.store(other.shutdown_flag.load());
            
//             this->cv.notify_all(); // Wake any threads waiting on the old state
//         }
//         return *this;
//     }

// //     void set_size(int size) {
// //         max_size = size;
// //     }

//     bool push(T&& data) {
//         std::unique_lock<std::mutex> lock(m);
//         if (shutdown_flag || queue.size() >= max_size) return false;

//         bool was_empty = queue.empty();
//         queue.emplace(std::forward<T>(data));
        
//         lock.unlock();
//         cv.notify_one();
//         return was_empty;
//     }

//     std::pair<bool, T> try_pop() {
//         std::unique_lock<std::mutex> lock(m);
//         if (queue.empty()) return {false, T{}};
//         T data = std::move(queue.front());
//         queue.pop();
//         return {true, std::move(data)};
//     }

//     T pop() {
//         std::unique_lock<std::mutex> lock(m);
//         cv.wait(lock, [this] { return !queue.empty() || shutdown_flag; });
//         if (queue.empty()) return T{};
//         T data = std::move(queue.front());
//         queue.pop();
//         return data;
//     }
//     void shutdown()
//     {
//         std::unique_lock<std::mutex> lock(m);
//         shutdown_flag = true;
//         cv.notify_all();
//     }
// };
// #endif




#ifndef BLCK_QUE
#define BLCK_QUE

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

template <typename T>
class Blocking_Queue {
private:
    // Using a pointer to a vector ensures that the data block 
    // stays at the same memory address even if the queue object is moved.
    std::unique_ptr<std::vector<T>> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    size_t capacity = 1024;
    
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> shutdown_flag{false};

public:
    Blocking_Queue() : buffer(std::make_unique<std::vector<T>>(1024)) {}

    // Proper Move Constructor
    Blocking_Queue(Blocking_Queue&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.m);
        this->buffer = std::move(other.buffer);
        this->head = other.head;
        this->tail = other.tail;
        this->count = other.count;
        this->capacity = other.capacity;
        this->shutdown_flag.store(other.shutdown_flag.load());
    }

    void set_size(int n) {
        std::unique_lock<std::mutex> lock(m);
        capacity = n;
        buffer = std::make_unique<std::vector<T>>(n);
    }

    bool push(T&& data) {
        std::unique_lock<std::mutex> lock(m);
        if (count == capacity || shutdown_flag) return false;

        bool was_empty = (count == 0);
        (*buffer)[tail] = std::move(data);
        tail = (tail + 1) % capacity;
        count++;

        lock.unlock();
        cv.notify_one();
        return was_empty;
    }

    // This is the "Drain Loop" helper
    std::pair<bool, T> try_pop() {
        std::unique_lock<std::mutex> lock(m);
        if (count == 0) return {false, T{}};
        
        T data = std::move((*buffer)[head]);
        head = (head + 1) % capacity;
        count--;
        return {true, std::move(data)};
    }

    T pop() {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this] { return count > 0 || shutdown_flag; });
        if (shutdown_flag && count == 0) return T{};

        T data = std::move((*buffer)[head]);
        head = (head + 1) % capacity;
        count--;
        return data;
    }

    void shutdown()
    {
        std::unique_lock<std::mutex> lock(m);
        shutdown_flag = true;
        cv.notify_all();
    }
};
#endif