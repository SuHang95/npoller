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
class locally_queue {
private:
    std::atomic<uint64_t> index;
    std::deque<std::pair<T, uint64_t>> local_queue;
    std::thread::id thread_id;
    //the index of last consumed item
    uint64_t consumed_index;
    char padding[64 - 8 - sizeof(std::deque<T>) - sizeof(std::thread::id) - 8];
    tbb::concurrent_queue<std::pair<T, uint64_t>> concurrent_queue;
public:
    explicit locally_queue(std::thread::id local_thread_id, size_t initial_size) :
            local_queue(initial_size), index(0), thread_id(local_thread_id), consumed_index(-1) {}

    explicit locally_queue(size_t initial_size = 0) : locally_queue(std::this_thread::get_id(), initial_size) {}

    void push(const T &source) {
        uint64_t l_index = index.fetch_add(1);
        if (std::this_thread::get_id() == thread_id) {
            local_queue.push_back(std::make_pair<T, uint64_t>(source, index));
        } else {
            queue.push(std::make_pair<T, uint64_t>(source, index));
        }
    }

    void push(T &&source) {
        uint64_t l_index = index.fetch_add(1);
        if (std::this_thread::get_id() == thread_id) {
            local_queue.emplace_back(source, index);
        } else {
            queue.push(std::move(std::make_pair<T, uint64_t>(std::move(source), std::move(index))));
        }
    }

    bool try_pop(T &destination) throw {
        if (std::this_thread::get_id() != thread_id) {
            std::stringstream ss;
            ss << "Try to pop value of locally_queue in thread " << std::this_thread::get_id() <<
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
            std::pair<T, uint64_t> item;
            int loop_time = 0;
            bool succ = false;
            bool loop = ((consumed_index+1) < index.load(std::memory_order_relaxed));
            do {
                succ = concurrent_queue.try_pop(item);
                if (succ && (item.second == consumed_index + 1)) {
                    return std::move(item.first);
                }

                if (succ) {
                    throw std::runtime_error("Disordered item in local queue!");
                }
            } while (loop && loop_time++ < 10);
        }
    }

};


#endif //NPOLLER_LOCALLY_QUEUE_H
