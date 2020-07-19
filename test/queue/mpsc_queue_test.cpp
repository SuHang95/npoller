//
// Created by suhang on 20-7-19.
//


#include <queue/mpsc_queue.h>
#include <vector>
#include <iostream>


size_t push_size = 0x100000;

int64_t test_mpsc_queue(int thread_size);

int64_t test_tbb_queue(int thread_size);

int main() {

    int64_t locally_cost = test_mpsc_queue(1);

    std::cout << "Test mpsc_queue with thread size=" << 2 << " and queue size=" << push_size <<
              " with " << (locally_cost / 1000) << " milliseconds!" << std::endl;

    locally_cost = test_mpsc_queue(3);

    std::cout << "Test mpsc_queue with thread size=" << 3 << " and queue size=" << push_size <<
              " with " << (locally_cost / 1000) << " milliseconds!" << std::endl;

    int64_t tbb_cost = test_tbb_queue(1);

    std::cout << "Test tbb::concurrent_queue with thread size=" << 2 << " and queue size=" << push_size <<
              " with " << (tbb_cost / 1000) << " milliseconds!" << std::endl;

    tbb_cost = test_tbb_queue(3);

    std::cout << "Test tbb::concurrent_queue with thread size=" << 3 << " and queue size=" << push_size <<
              " with " << (tbb_cost / 1000) << " milliseconds!" << std::endl;

    return 0;
}

void accept(std::vector<bool> &accepted, size_t j) {
    accepted[j] = true;
}

//return time
int64_t test_mpsc_queue(int thread_size) {
    mpsc_queue<size_t> queue;
    std::vector<std::thread> threads;
    std::vector<bool> accepted(push_size, false);
    size_t accept_size = 0;

    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    for (int i = 0; i < thread_size; i++) {
        threads.emplace_back([&](int d) {
            int base = 2 * thread_size;
            for (size_t j = 0; j < push_size / base; j++) {
                queue.push(j * base + 2 * d + 1);
            }
            if (2 * d + 1 < (push_size % base)) {
                queue.push((push_size / base) * base + 2 * d + 1);
            }
        }, i);
    }

    for (size_t i = 0; i < push_size / 2; i++) {
        size_t j = 0;
        queue.push(2 * i);
        if (queue.try_pop(j)) {
            accept(accepted, j);
            accept_size++;
        }
    }

    while (accept_size < push_size) {
        size_t j = 0;
        if (queue.try_pop(j)) {
            accept(accepted, j);
            accept_size++;
        }
    }

    std::chrono::microseconds _end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );

    for (size_t i = 0; i < push_size; i++) {
        if (accepted[i] != true) {
            std::cout << "The " << i << "th items' result is not as expected!";
            exit(1);
        }
    }

    for (auto &t:threads) {
        t.join();
    }

    return _end.count() - _start.count();
}

//return time
int64_t test_tbb_queue(int thread_size) {
    tbb::concurrent_queue<size_t> queue;
    std::vector<std::thread> threads;
    std::vector<bool> accepted(push_size, false);
    size_t accept_size = 0;

    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    for (int i = 0; i < thread_size; i++) {
        threads.emplace_back([&](int d) {
            int base = 2 * thread_size;
            for (size_t j = 0; j < push_size / base; j++) {
                queue.push(j * base + 2 * d + 1);
            }
            if (2 * d + 1 < (push_size % base)) {
                queue.push((push_size / base) * base + 2 * d + 1);
            }
        }, i);
    }

    for (size_t i = 0; i < push_size / 2; i++) {
        size_t j = 0;
        queue.push(2 * i);
        if (queue.try_pop(j)) {
            accept(accepted, j);
            accept_size++;
        }
    }

    while (accept_size < push_size) {
        size_t j = 0;
        if (queue.try_pop(j)) {
            accept(accepted, j);
            accept_size++;
        }
    }

    std::chrono::microseconds _end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );

    for (size_t i = 0; i < push_size; i++) {
        if (accepted[i] != true) {
            std::cout << "The " << i << "th items' result is not as expected!";
            exit(1);
        }
    }

    for (auto &t:threads) {
        t.join();
    }

    return _end.count() - _start.count();
}