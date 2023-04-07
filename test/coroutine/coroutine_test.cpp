//
// Created by suhang on 23-4-5.
//
#include <iostream>
#include <coroutine_pool//threadpool.h>
#include <coroutine_pool/coroutine_pool.h>

std::atomic_int64_t times;
std::atomic_int64_t times2;
std::promise<void> promise;
std::future<void> fut = promise.get_future();
int64_t count = 10000000;

coroutine_task<void> add_task(coroutine_pool &pool) {
    int a= (co_await pool.submit_task_end([](int a) -> coroutine_task<int> {
        times2.fetch_add(1);
        co_return a;
    }, 1));
    if (times.fetch_add(a) == (count - 1)) {
        if (times.load() == count)
            promise.set_value();
    }
    co_return;
}


int main() {
    coroutine_pool pool;
    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());

    for (size_t i = 0; i < count; i++) {
        pool.enqueue([&pool]() {
            auto t = add_task(pool);
            t.h_.resume();
        });
    }
    fut.get();

    std::chrono::microseconds _end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    std::cout << "times:" << _end.count() - _start.count() << std::endl;
}