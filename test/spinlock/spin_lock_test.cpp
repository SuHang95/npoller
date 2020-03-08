//
// Created by suhang on 19-4-27.
//

#include<spinlock/spin_lock.h>
#include<iostream>
#include<vector>

int l;
int k;
int l2;

void reentrant_test() {
    static thread_local int f_count = 0;
    static reentrant_spin_lock a;

    if (f_count == 0) {
        k++;
    }

    if (f_count++ < 4) {
        a.lock();
        l++;
        reentrant_test();
        a.unlock();
    } else {
        l++;
        std::cout << "Thread " << std::this_thread::get_id() << "run!" << std::endl;
    }
}


void test() {
    static spin_lock a;

    k++;

    {
        a.lock();
        l2++;
        a.unlock();
    }
}


int main() {
    std::vector<std::thread> threads,threads2;

    for (size_t i = 0; i < 1000; i++) {
        threads.emplace_back(reentrant_test);
    }

    for (auto &t:threads) {
        t.join();
    }

    for (size_t i = 0; i < 1000; i++) {
        threads2.emplace_back(test);
    }

    for (auto &t:threads2) {
        t.join();
    }

    std::cout<<"k= "<<k<<"l= "<<l<<" l2="<<l2<<std::endl;

    return 0;
}