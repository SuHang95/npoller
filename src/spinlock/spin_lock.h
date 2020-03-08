//
// Created by suhang on 19-4-27.
//

#ifndef NETPROCESSOR_SPIN_LOCK_H
#define NETPROCESSOR_SPIN_LOCK_H

#include <atomic>
#include <thread>
#include <iostream>
#include <x86intrin.h>


//an spin lock
class spin_lock {
public:
    spin_lock() : locked(false) {}

    spin_lock(const spin_lock &) = delete;

    spin_lock(spin_lock &&) = delete;

    spin_lock &operator=(const spin_lock &) = delete;

    spin_lock &operator=(spin_lock &&) = delete;


    inline void lock() {
        int pause_times;
        bool default_val = false;
        //当前循环pause次数
        int k = 1;

        while (!locked.compare_exchange_strong(default_val, true, std::memory_order_acquire)) {
            default_val = false;
            while (1) {
                _mm_pause();//  pause指令 延迟时间大约是12纳秒
                bool temp = locked.load(std::memory_order_relaxed);
                if (!temp) {
                    break;
                }
                k *= 2;
                for (size_t j = 0; j <= k; j++) {
                    _mm_pause();
                }
                if (k == 2048) {
                    k = 2;
                }
            }
        }
    }

    inline bool try_lock() {
        bool default_val = false;

        return locked.compare_exchange_strong(default_val, true, std::memory_order_acquire);
    }

    inline void unlock() {
        bool expected_locked_val = true;

        if (!locked.compare_exchange_strong(expected_locked_val, false, std::memory_order_release)) {
            throw std::runtime_error("Try to unlock an unlocked lock!");
        }
    }

private:
    std::atomic<bool> locked;
};

//an spin,reentrant lock
class reentrant_spin_lock {
public:
    reentrant_spin_lock() : thread_id(default_id),
                            count(0) {};

    reentrant_spin_lock(const reentrant_spin_lock &) = delete;

    reentrant_spin_lock(reentrant_spin_lock &&) = delete;

    reentrant_spin_lock &operator=(const reentrant_spin_lock &) = delete;

    reentrant_spin_lock &operator=(reentrant_spin_lock &&) = delete;


    inline void lock() {
        std::thread::id id = std::this_thread::get_id();
        std::thread::id contained = default_id;
        int pause_times;
        //当前循环pause次数
        int k = 1;

        while (!(thread_id.compare_exchange_strong(contained, id, std::memory_order_acquire) ||
                 contained == id)) {
            contained = default_id;
            while (1) {
                _mm_pause();//  pause指令 延迟时间大约是12纳秒
                std::thread::id temp = thread_id.load(std::memory_order_relaxed);
                if (temp == id || temp == default_id) {
                    break;
                }
                k *= 2;
                for (size_t j = 0; j <= k; j++) {
                    _mm_pause();
                }
                if (k == 2048) {
                    k = 2;
                }
            }
        }

        count++;
    }

    inline bool try_lock() {
        std::thread::id id = std::this_thread::get_id();
        std::thread::id contained = default_id;

        if (thread_id.compare_exchange_strong(contained, id, std::memory_order_acquire) ||
            contained == id) {
            count++;
        } else {
            return false;
        }

        return true;
    }

    inline void unlock() {
        std::thread::id id = std::this_thread::get_id();
        if (id != thread_id.load(std::memory_order_relaxed)) {
            throw std::runtime_error("Try to unlock an lock which doesn't belongs to this thread or an unlocked lock!");
        }

        if (--count == 0) {
            thread_id.store(default_id, std::memory_order_release);
        }
    }

private:
    int count;
    std::atomic<std::thread::id> thread_id;
public:
    const static std::thread::id default_id;
};


#endif //NETPROCESSOR_SPIN_LOCK_H
