#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <deque>
#include <atomic>
#include <mutex>
#include <iostream>
#include <boost/lockfree/queue.hpp>
#include <shared_mutex>

#define DEFAULT_SIZE 0x200000

const size_t ONE_GIGABYTE = 0x40000000;

const size_t MIN_CAPACITY = 0x100;

//lock-free queue in mem_container's init size
const size_t INIT_SIZE = 0x100;

class mem_container {
public:
    mem_container(size_t size = ONE_GIGABYTE / DEFAULT_SIZE) :
            container(INIT_SIZE) {
        max_size = size > MIN_CAPACITY ? size : MIN_CAPACITY;
    }

    mem_container(const mem_container &) = delete;

    mem_container(mem_container &&) = delete;

    mem_container &operator=(const mem_container &) = delete;

    mem_container &operator=(mem_container &&) = delete;

    void inline set_max(size_t size) {
        max_size = size > MIN_CAPACITY ? size : MIN_CAPACITY;
    }

    inline char *get() {
        std::shared_lock lock(mutex_);
        char *ret;

        if (!container.empty()) {
            container.pop(ret);
            size.fetch_sub(1, std::memory_order_relaxed);
            return ret;
        } else {
            return new char[DEFAULT_SIZE];
        }
    };

    inline void push(char *ptr) {
        std::shared_lock lock(mutex_);
        if (size.load(std::memory_order_relaxed) <= max_size) {
            container.push(ptr);
            size.fetch_add(1, std::memory_order_relaxed);
        } else {
            delete[]ptr;
        }
    }

    ~mem_container() {
        char *ret;
        std::unique_lock lock(mutex_);
        while (!container.empty()) {
            container.pop(ret);
            delete[]ret;
        }
    }

private:
    boost::lockfree::queue<char *> container;
    std::shared_mutex mutex_;
    size_t max_size;
    std::atomic<int> size;
};


#endif
