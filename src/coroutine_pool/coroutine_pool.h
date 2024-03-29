//
// Created by suhang on 23-4-4.
//

#ifndef NPOLLER_COROUTINE_POOL_H
#define NPOLLER_COROUTINE_POOL_H

//
// Created by suhang on 23-3-30.
//

#include "threadpool.h"
#include <coroutine>
#include <optional>
#include <deque>
#include <list>
#include <cassert>
#include <iostream>
#include <tbb/concurrent_queue.h>

struct wait_with_coroutine_start {
};

struct wait_with_coroutine_end {
};

class coroutine_pool;

template<typename T>
class coroutine_task {
public:
    struct promise_type;

    typedef T type;

    coroutine_task(const std::coroutine_handle<promise_type> &h, const std::shared_future<T> &future) : future_(
            future) {
        h_ = h;
    }

    coroutine_task(const coroutine_task &other) : future_(other.future_), h_(other.h_) {}

    coroutine_task(coroutine_task &&other) : future_(std::move(other.future_)), h_(other.h_) {}

    coroutine_task &operator=(coroutine_task &&other) {
        if (this != &other) {
            future_ = std::move(other.future_);
            h_ = other.h_;
        }
        return *this;
    }

    struct promise_type : std::promise<T> {
        coroutine_task get_return_object() {
            return coroutine_task(std::coroutine_handle<promise_type>::from_promise(*this), this->get_future().share());
        }

        std::suspend_always initial_suspend() {
            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }


        void return_value(const T &value)
        noexcept(std::is_nothrow_copy_constructible_v<T>) {
            this->set_value(value);
            resume_all_waitor(false);
        }

        void
        return_value(T &&value)
        noexcept(std::is_nothrow_move_constructible_v<T>) {
            this->set_value(std::move(value));
            resume_all_waitor(false);
        }

        template<typename R>
        struct result_awaitable {
        public:
            explicit result_awaitable(const coroutine_task<R> & task) : coroutineTask(task) {}

            explicit result_awaitable(coroutine_task<R> && task) : coroutineTask(task) {}

            bool await_ready() {
                return coroutineTask.done();
            }

            bool await_suspend(std::coroutine_handle<> h) {
                if (!coroutineTask.add_waitor(h)) {
                    result = coroutineTask();
                    return false;
                }
                return true;
            }

            R &await_resume() {
                return result.has_value() ? *result : coroutineTask();
            }

        private:
            coroutine_task<R> coroutineTask;
            std::optional<R> result;
        };


        template<typename U>
        U&& await_transform(U&& awaitable) noexcept
        {
            return static_cast<U&&>(awaitable);
        }

        template<class R>
        result_awaitable<R>& await_transform(coroutine_task<R>&& task) noexcept{
            return result_awaitable(task);
        }

        void unhandled_exception() {
            std::cout << "Exception happen!" << std::current_exception << std::endl;
        }

        bool add_waitor(std::coroutine_handle<> h) {
            if (finish_.load(std::memory_order_relaxed)) {
                return false;
            }
            int retry_times = 0;
            {
                do {
                    if (!mutex_.try_lock()) {
                        if (finish_.load(std::memory_order_relaxed)) {
                            return false;
                        }
                        continue;
                    }
                    //try lock success
                    //this logic is same to double-check singleton
                    if (!finish_.load(std::memory_order_relaxed)) {
                        waiters.push_back(h);
                        mutex_.unlock();
                        return true;
                    }
                    mutex_.unlock();
                    return false;
                } while (retry_times++ < 3);
            }
            return false;
        }

        void resume_all_waitor(bool finish) {
            std::lock_guard<std::mutex> lck(mutex_);
            if (finish) {
                finish_.store(finish, std::memory_order_relaxed);
            }
            while (!waiters.empty()) {
                waiters.front().resume();
                waiters.pop_front();
            }
        }

        ~promise_type() {
            resume_all_waitor(true);
        }

        std::list<std::coroutine_handle<>> waiters;
        std::mutex mutex_;
        std::atomic_bool finish_;
    };


    void resume() {
        h_.resume();
    }


    bool done() {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    bool add_waitor(std::coroutine_handle<> h) {
        if (!done() && h_.promise().add_waitor(h)) {
            return true;
        } else {
            return false;
        }
    }

    T operator()() {
        if (future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            std::cout << "Bad situation2!" << std::endl;
        }
        return future_.get();
    }

private:
    std::coroutine_handle<promise_type> h_;
    std::shared_future<T> future_;
};

template<>
struct coroutine_task<void> {
    struct promise_type : std::promise<void> {
        coroutine_task get_return_object() {
            return coroutine_task{std::coroutine_handle<promise_type>::from_promise(*this), this->get_future()};
        }

        std::suspend_always initial_suspend() {
            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void return_void() {
            this->set_value();
        }

        void unhandled_exception() {}
    };

    void resume() {
        h_.resume();
    }

    bool done() {
        return h_.done();
    }

    void operator()() {
        return future_.get();
    }

    std::coroutine_handle<promise_type> h_;
    std::future<void> future_;
};

class coroutine_pool : public thread_pool {
public:
    coroutine_pool() : endflag(std::make_shared<std::atomic_bool>(false)) {}

    coroutine_pool(size_t size) : thread_pool(size), endflag(std::make_shared<std::atomic_bool>(false)) {}

    coroutine_pool(const coroutine_pool &) = delete;

    coroutine_pool(coroutine_pool &&) = delete;

    ~coroutine_pool() {
        endflag->store(true, std::memory_order_relaxed);
    }

    template<class T>
    class execute_callback {
    public:
        execute_callback() : has_ran(false) {}

        execute_callback(const execute_callback &) = delete;

        execute_callback(execute_callback &&) = delete;

        void operator()(coroutine_task<T> &t) {
            std::lock_guard<std::mutex> lck(mutex);
            has_ran.store(true, std::memory_order_relaxed);
            if (callback.has_value()) {
                callback.value().operator()(t);
            }
        }

        bool set_callback(std::function<void(coroutine_task<T> &)> &&func) {
            if (has_ran.load(std::memory_order_relaxed)) {
                return false;
            }
            int retry_times = 0;
            {
                do {
                    if (!mutex.try_lock()) {
                        if (has_ran.load(std::memory_order_relaxed)) {
                            return false;
                        }
                        continue;
                    }
                    //try lock success
                    //this logic is same to double-check singleton
                    if (!has_ran.load(std::memory_order_relaxed) && !callback.has_value()) {
                        callback = std::move(func);
                        mutex.unlock();
                        return true;
                    }
                    mutex.unlock();
                    return false;
                } while (retry_times++ < 3);
            }
            return false;
        }

    private:
        std::mutex mutex;
        std::optional<std::function<void(coroutine_task<T> &t)>> callback;
        std::atomic_bool has_ran;
    };

    template<typename T, typename wait_timepoint>
    class enqueue_awaitable {
    public:
        enqueue_awaitable(std::future<coroutine_task<T>> &&_future, std::shared_ptr<execute_callback<T>> callback,
                          coroutine_pool *pool)
                : future_(std::move(_future)), callback_(callback), pool_(pool) {}

        enqueue_awaitable(const enqueue_awaitable &) = delete;

        enqueue_awaitable operator=(const enqueue_awaitable &) = delete;

        enqueue_awaitable(enqueue_awaitable &&other) : future_(std::move(other.future_)),
                                                       return_value(std::move(other.return_value)) {}

        enqueue_awaitable operator=(enqueue_awaitable &&other) {
            if (this != &other) {
                future_ = std::move(other.future_);
                return_value = std::move(other.return_value);
            }
            return *this;
        }

        bool await_ready() {
            if (!return_value.has_value()) {
                bool is_ready = future_.
                        wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                //if the coroutine handle is ready, then we will call the future_.get()
                if (!is_ready) {
                    return false;
                }
                return_value = std::move(future_.get());
            }

            if constexpr (std::is_same<wait_timepoint, wait_with_coroutine_start>::value) {
                return true;
            } else {
                return return_value.value().done();
            }
        }

        bool await_suspend(std::coroutine_handle<> h) {
#ifdef _DEBUG
            pool_->h_resume_count.fetch_add(1);
#endif
            bool callback_set = callback_->set_callback([h, this](coroutine_task<T> &t) {
                //only if callback set successfully, this lambda will run and in this case
                //at least one of the thread in thread pool is still running, the thread pool object is still valid

                //TODO PAY MORE ATTENTION ON THIS
                if constexpr (std::is_same<wait_timepoint, wait_with_coroutine_start>::value) {
                    if (pool_->is_busy()) {
                        h.resume();
                        return;
                    }
                    pool_->add_runnable(h);
                } else {
                    if (!t.add_waitor(h)) {
                        pool_->add_runnable(h);
                    }
                }

            });
            //no matter whether coroutine we enqueued has completed, we can ensure the current coroutine will run
            // if the callback set successfully
            if (!callback_set) {
                int count = 0;
                while (!await_ready()) {
                    count++;
                    if (count > 3) {
                        if (!return_value.has_value()) {
                            return_value = std::move(future_.get());
                        }
                    }
#ifdef _DEBUG
                    if (count > 0) {
                        pool_->bad_situation1.fetch_add(1, std::memory_order_relaxed);
                        std::cout << "count = " << count << std::endl;
                    }
#endif
                }

                return false;
            }
            return true;
        }

        auto await_resume() {
#ifdef _DEBUG
            pool_-> return_count1.fetch_add(1);
#endif
            if constexpr (std::is_same<wait_timepoint, wait_with_coroutine_start>::value) {
                return return_value.has_value() ? *return_value : future_.get();
            } else {
#ifdef _DEBUG
                if (!return_value.has_value() && future_.
                        wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                    pool_->bad_situation.fetch_add(1);
                }
#endif
                return return_value.has_value() ? return_value.value()() : future_.get()();
            }
        }


    private:

        std::future<coroutine_task<T>> future_;

        std::optional<coroutine_task<T>> return_value;

        std::shared_ptr<execute_callback<T>> callback_;
        //Be careful of this
        coroutine_pool *pool_;
    };

    template<typename wait_timepoint, typename routine_func, typename ...arg>
    auto submit_task(routine_func &&func, arg &&...args) {
        using return_type = typename std::result_of<routine_func(arg...)>::type;
        using T = typename return_type::type;
        std::shared_ptr<execute_callback<T>> callback_ptr = std::make_shared<execute_callback<T>>();
        auto function = std::bind(std::forward<routine_func>(func), std::forward<arg...>(args...));
        std::promise<coroutine_task<T>> promise;
        std::future<return_type> future = promise.get_future();

        auto task = std::make_shared<std::packaged_task<void()> >(
                [_function = std::move(function), callback_ptr, _promise = std::move(promise)]() mutable {
                    //this sequence is very important!!! do not change if you don't know what you are doing now
                    auto ret = _function();
                    _promise.set_value(ret);
                    (*callback_ptr)(ret);
                    ret.resume();
                }
        );

        m_tasks.push([=]() {
            (*task)();
        });

        return enqueue_awaitable<T, wait_timepoint>(std::move(future), callback_ptr, this);
    }

    template<typename routine_func, typename ...arg>
    auto submit_task_start(routine_func &&func, arg &&...args) {
        return submit_task<wait_with_coroutine_start, routine_func, arg...>(std::forward<routine_func>(func),
                                                                            std::forward<arg...>(args...));
    }

    template<typename routine_func, typename ...arg>
    auto submit_task_end(routine_func &&func, arg &&...args) {
        return submit_task<wait_with_coroutine_end, routine_func, arg...>(std::forward<routine_func>(func),
                                                                          std::forward<arg...>(args...));
    }

    void add_runnable(std::coroutine_handle<> h) {
        this->enqueue([h, this]() {
            h.resume();
        });
    }

private:
    std::shared_ptr<std::atomic_bool> endflag;

    bool is_busy() {
        return m_tasks.size() >= ((workers.size() / 2) * 3);
    }

#ifdef _DEBUG
    std::atomic_int64_t bad_situation;
    std::atomic_int64_t bad_situation1;
    std::atomic_int64_t h_resume_count;
    std::atomic_int64_t return_count1;
    std::atomic_int64_t return_count2;
#endif
};


#endif //NPOLLER_COROUTINE_POOL_H
