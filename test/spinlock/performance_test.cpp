//
// Created by suhang on 19-4-28.
//
#include<spinlock/spin_lock.h>
#include<iostream>
#include<random>
#include<mutex>

spin_lock m;
size_t k;
int64_t ftime;
int64_t gtime;

void f() {
    int *a = (int *) malloc(0x10000 * sizeof(int));
    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    for (int j = 0; j < 1000; j++) {
        for (size_t i = 0; i < 0x10000; i++) {
            m.lock();
            k += a[i];
            a[(i + i * j) % 0x10000] = k;
            m.unlock();
        }
    }
    std::chrono::microseconds _end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    free(a);
    m.lock();
    ftime += _end.count() - _start.count();
    m.unlock();
}

void g() {
    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    int *a = (int *) malloc(0x10000 * sizeof(int));
    for (int j = 0; j < 1000; j++) {
        for (size_t i = 0; i < 0x10000; i++) {
            k += a[i];
            a[(i + i * j) % 0x10000] = k;
        }
    }
    std::chrono::microseconds _end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    );
    free(a);
    gtime += _end.count() - _start.count();
}

int main() {
    for (int i = 0; i < 1; i++) {
        g();
    }
    std::cout << "result=" << k << ",use time=" << gtime << std::endl;

    std::vector<std::thread> v;
    for (int i = 0; i < 1; i++) {
        v.emplace_back(f);
    }

    for (auto &t:v) {
        t.join();
    }
    std::cout << "result=" << k << ",use time=" << ftime << std::endl;

    return 0;
}
