//
// Created by suhang on 23-4-4.
//

#ifndef NPOLLER_THREADPOOL_H
#define NPOLLER_THREADPOOL_H


#include<queue>
#include<thread>
#include<future>
#include<vector>
#include<array>
#include<atomic>
#include<functional>
#include<tbb/concurrent_queue.h>


/*class thread {
public:
    thread();

    template<typename func, typename ...arg>
    explicit
    thread(func &&callable, arg &&... args):m_thread(
            std::forward<func>(callable),
            std::forward<arg>(args)...) {}

    thread(const thread &) = delete;

    thread(thread &&) = delete;

    inline void join();

    virtual ~thread();

    template<typename func, typename ...arg>
    auto enqueue(func &&f, arg &&... args) -> std::future<typename std::result_of<func(arg...)>::type> {
        using return_type = typename std::result_of<func(arg...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()> >(
                std::bind(std::forward<func>(f), std::forward<arg...>(args)...)
        );

        auto future = task->get_future();
        {
            std::unique_lock<std::mutex> lck(m_mutex);
            m_tasks.emplace([task]() {
                (*task)();
            });
        }
        m_cv.notify_one();

        return future;
    }

    template<typename func>
    auto enqueue(func &&f) -> std::future<typename std::result_of<func()>::type> {
        using return_type = typename std::result_of<func()>::type;

        auto task = std::make_shared<std::packaged_task<return_type()> >(
                std::bind(std::forward<func>(f))
        );

        auto future = task->get_future();
        {
            std::unique_lock<std::mutex> lck(m_mutex);
            m_tasks.emplace([task]() {
                (*task)();
            });
        }
        m_cv.notify_one();

        return future;
    }

private:
    std::atomic_bool construct;
    std::thread m_thread;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool stop;
};*/


class thread_pool {
public:
    thread_pool(std::size_t size = std::thread::hardware_concurrency());

    thread_pool(const thread_pool &) = delete;

    thread_pool(thread_pool &&) = delete;

    virtual ~thread_pool();

    template<typename func, typename ...arg>
    auto enqueue(func &&, arg &&...) -> std::future<typename std::result_of<func(arg...)>::type>;

    template<typename func>
    auto enqueue(func &&) -> std::future<typename std::result_of<func()>::type>;

protected:
    std::vector<std::thread> workers;
    tbb::concurrent_bounded_queue<std::function<void()>> m_tasks;
    std::atomic_bool stop;
};


/*su::thread::thread() : stop(false), m_thread([this]() {
    while (construct.load() == false) {
        usleep(100);
    }

    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lck(m_mutex);
            m_cv.wait(lck, [this]() {
                return this->stop || !this->m_tasks.empty();
            });

            if (this->stop && m_tasks.empty())
                return;
            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
        }

        task();
    }
}) {
    construct = true;
}


inline void su::thread::join() {
    {
        std::unique_lock<std::mutex> unique_lock(m_mutex);
        stop = true;
    }

    m_cv.notify_one();
    m_thread.join();
}

su::thread::~thread() {
    join();
}*/


thread_pool::thread_pool(std::size_t size) : m_tasks(), stop(false) {
    for (size_t i = 0; i < size; i++) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    m_tasks.pop(task);
                }
                task();
                if (stop == true) {
                    while (m_tasks.try_pop(task)) {
                        task();
                    }
                    return;
                }
            }

        });
    }
#ifdef _DEBUG
    workers.emplace_back([this]() {
        while (true) {
            if (stop == true) {
                break;
            }
            sleep(1);
            std::cout << m_tasks.size() << std::endl;
        }
    });
#endif
}

thread_pool::~thread_pool() {
    stop = true;
    for (int i = 0; i < 4 * workers.size(); i++) {
        m_tasks.push([]() {});
    }
    for (auto &thread: workers) {
        thread.join();
    }
}

template<typename func, typename ...arg>
auto thread_pool::enqueue(func &&f, arg &&... args) -> std::future<typename std::result_of<func(arg...)>::type> {
    using return_type = typename std::result_of<func(arg...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<func>(f), std::forward<arg...>(args)...)
    );

    auto future = task->get_future();
    {
        m_tasks.push([task]() {
            (*task)();
        });
    }
    return future;
}

template<typename func>
auto thread_pool::enqueue(func &&f) -> std::future<typename std::result_of<func()>::type> {
    using return_type = typename std::result_of<func()>::type;

    auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<func>(f))
    );

    auto future = task->get_future();
    {
        m_tasks.push([task]() {
            (*task)();
        });
    }
    return future;
}


#endif //NPOLLER_THREADPOOL_H
