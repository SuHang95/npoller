//
// Created by suhang on 20-7-12.
//

#ifndef NPOLLER_LOCALLY_QUEUE_H
#define NPOLLER_LOCALLY_QUEUE_H

#include<deque>
#include<tbb/concurrent_queue.h>
#include<atomic>
#include<thread>
#include<sstream>

template<typename T>
class mpsc_queue {
private:
    std::atomic<uint64_t> index;
    std::deque<std::pair<T, uint64_t>> local_queue;
    std::thread::id thread_id;
    //the index of last consumed item
    uint64_t consumed_index;
    //char padding[64 - 8 - sizeof(std::deque<T>) - sizeof(std::thread::id) - 8];
    tbb::concurrent_queue<T> concurrent_queue;
public:
    explicit mpsc_queue(std::thread::id local_thread_id, size_t initial_size) :
            local_queue(initial_size), index(0), thread_id(local_thread_id), consumed_index(-1) {}

    explicit mpsc_queue(size_t initial_size = 0) : mpsc_queue(std::this_thread::get_id(), initial_size) {}

    void push(const T &source) {
        if (std::this_thread::get_id() == thread_id) {
            uint64_t l_index = index.fetch_add(1);
            local_queue.emplace_back(source, l_index);
        } else {
            index.fetch_add(1);
            concurrent_queue.emplace(source);
        }
    }

    void push(T &&source) {
        if (std::this_thread::get_id() == thread_id) {
            uint64_t l_index = index.fetch_add(1);
            local_queue.emplace_back(source, l_index);
        } else {
            index.fetch_add(1);
            concurrent_queue.emplace(std::move(source));
        }
    }

    bool try_pop(T &destination) {
        if (std::this_thread::get_id() != thread_id) {
            std::stringstream ss;
            ss << "Try to pop value of queue in thread " << std::this_thread::get_id() <<
               " with master thread of queue is " << thread_id;
            throw std::runtime_error(ss.str());
        } else {
            if (!local_queue.empty()) {
                if (consumed_index + 1 == local_queue[0].second) {
                    destination = std::move(local_queue[0].first);
                    local_queue.pop_front();
                    consumed_index++;

                    return true;
                }
            }
            T item;
            int loop_time = 0;
            bool success = false;
            /*uint64_t l_index = index.load(std::memory_order_relaxed);
            if (consumed_index + 1 == l_index) {
                return false;
            }*/

            //do {
                if (concurrent_queue.try_pop(destination)) {
                    consumed_index++;
                    return true;
                }
            //} while ((consumed_index + 1 < l_index) && loop_time++ < 3);

            return false;
        }
    }

};


#endif //NPOLLER_LOCALLY_QUEUE_H
