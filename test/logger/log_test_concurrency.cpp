#include<iostream>
#include<thread>
#include<vector>
#include<sulog/logger.h>
#include<random>

#ifdef __gnu_linux__

#include<unistd.h>

#endif

void swap(std::vector<logger> &array, int i, int j) {
    i = i % array.size();
    j = j % array.size();
    swap(array[i], array[j]);
}

void swap_test(std::vector<logger> *array_ptr, int i) {
    std::random_device rd;
    std::vector<logger> &array = *array_ptr;
    for (size_t i = 0; i < 0x1000000; ++i) {
        int a1 = rd();
        int a2 = rd();
        if ((a1 % array.size()) == (a2 % array.size())) {
            a2++;
        }
        swap(array, a1, a2);
    }
}

void log_test(std::vector<logger> *array_ptr, int i) {
    std::random_device rd;
    std::vector<logger> &array = *array_ptr;
    for (size_t j = 0; j < 0x100000; j++) {
        array[rd() % array.size()].info("This is %d thread,%d loops!", i, j);
    }
}


int main() {
    std::vector<logger> test_array;
    for (int i = 0; i < 100; ++i) {
        test_array.emplace_back("test" + std::to_string(i), logger::INFO, false, day);
    }

    std::vector<std::thread> threads;
    for (int j = 0; j < 8; ++j) {
        threads.emplace_back(swap_test, &test_array, j);
    }

    for (auto &thread: threads) {
        thread.join();
    }

    std::vector<std::thread> log_threads;
    for (int j = 0; j < 8; ++j) {
        log_threads.emplace_back(log_test, &test_array, j);
    }

    for (auto &thread: log_threads) {
        thread.join();
    }

    return 0;
}
