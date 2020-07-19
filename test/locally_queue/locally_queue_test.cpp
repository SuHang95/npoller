//
// Created by suhang on 20-7-19.
//


#include <locally_queue/locally_queue.h>
#include <vector>
#include <iostream>

int thread_size = 1;
size_t push_size = 0x100;

int64_t test_locally_queue();

int main() {

    int64_t locally_cost=test_locally_queue();

    std::cout<<"Test locally_queue with thread size="<<thread_size+1<<" and queue size="<<push_size<<
    " with "<<locally_cost<<" milliseconds!";

    return 0;
}

void accept(std::vector<bool> &accepted, size_t j) {
    accepted[j] = true;
}

//return time
int64_t test_locally_queue() {
    locally_queue<size_t> queue;
    std::vector<std::thread> threads;
    std::vector<bool> accepted(push_size, false);
    size_t accept_size = 0;

    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    for (int i = 0; i < thread_size; i++) {
        threads.emplace_back([&](int d) {
            for (size_t j = 0; j < push_size / (thread_size * 2); j++) {
                queue.push(j * (thread_size * 2) + 2 * d + 1);
            }
        }, i);
    }

    for (size_t i = 0; i < push_size / 2; i++) {
        size_t j = 0;
        queue.push(2*i);
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

    for(size_t i=0;i<push_size;i++){
        if(accepted[i]!=true){
            std::cout<<"The "<<i<<"th items' result is not as expected!";
            exit(1);
        }
    }

    for (auto &t:threads) {
        t.join();
    }

    return _end.count()-_start.count();
}