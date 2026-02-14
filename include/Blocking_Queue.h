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