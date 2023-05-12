#include <iobuffer/io_buffer.h>
#include <random>
#include <sulog/logger.h>
#include <array>

#define test_buff_size 69128
#define array_size 20

inline size_t __test_get_begin(io_buffer &test_buff) {
    if (test_buff.size() == 0)
        return 0;
    size_t i = 0;
    for (size_t j = 0; j < sizeof(size_t); j++) {
        ((char *) &i)[j] = test_buff.get(j);
    }
    return i;
}


inline size_t __test_get_last(io_buffer &test_buff) {
    if (test_buff.size() == 0)
        return 0;
    size_t i = 0;
    for (size_t j = 0; j < sizeof(size_t); j++) {
        ((char *) &i)[j] = test_buff.get(test_buff.size() - sizeof(size_t) + j);
    }
    return i;
}


inline size_t __test_get_with_index(io_buffer &test_buff, size_t index) {
    if (test_buff.size() == 0)
        return 0;
    size_t i = 0;
    for (size_t j = 0; j < sizeof(size_t); j++) {
        ((char *) &i)[j] = test_buff.get(index * sizeof(size_t) + j);
    }
    return i;
}


inline char *__test_get_num_ptr(size_t start, size_t n) {
    size_t *ptr = new size_t[n];
    size_t j = start;
    for (size_t i = 0; i < n; i++) {
        ptr[i] = j++;
    }
    return (char *) ptr;
}

inline void __test_print(io_buffer &test_buff) {
    if (test_buff.size() != 0) {
        for (size_t j = 0; j < test_buff.size() / sizeof(size_t); j++) {
            size_t i = 0;
            for (size_t m = 0; m < sizeof(size_t); m++) {
                ((char *) &i)[m] = test_buff.get(j * sizeof(size_t) + m);
            }
            std::cout << i << " ";
            if (j % 8 == 7) {
                std::cout << std::endl;
            }
        }
    }
}

/**
 * @param buff_array    用于测试的buffer数组
 * @param log           日志
 * @param f             测试的方法
 * @param incr          测试buffer index是否递增
 * @param test_info     测试的名称
 */
void data_other_test(std::array<io_buffer, array_size> buff_array, logger log,
                     void (io_buffer::*f)(io_buffer &, size_t),
                     bool incr, const std::string &test_info) {
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> m(0, 0x10000);

    size_t index = 0;
    for (int i = 0; i < array_size; i++) {
        size_t next_index = (index + array_size + (incr ? 1 : -1)) % array_size;

        /*_log.print("Before %s:buff array[%d] begin=%zd,last=%zd,size=%zd", test_info.c_str(),
                  index, __test_get_begin(buff_array[index]),
                  __test_get_last(buff_array[index]), buff_array[index].size());
        _log.print("Before %s:buff array[%d] begin=%zd,last=%zd,size=%zd", test_info.c_str(),
                  next_index, __test_get_begin(buff_array[next_index]),
                  __test_get_last(buff_array[next_index]), buff_array[next_index].size());*/

        size_t pop_size = m(gen) % test_buff_size;
        (buff_array[index].*f)(buff_array[next_index],
                               pop_size * sizeof(size_t));

        /*_log.print("%s %zd bytes!,%zd num", test_info.c_str(), pop_size * sizeof(size_t), pop_size);
        _log.print("After %s:buff array[%d] begin=%zd,last=%zd,size=%zd", test_info.c_str(),
                  index, __test_get_begin(buff_array[index]),
                  __test_get_last(buff_array[index]), buff_array[index].size());
        _log.print("After %s:buff array[%d] begin=%zd,last=%zd,size=%zd\n", test_info.c_str(),
                  next_index, __test_get_begin(buff_array[next_index]),
                  __test_get_last(buff_array[next_index]), buff_array[next_index].size());*/

        index = next_index;
    }
}

/**
 * @param buff_array    用于测试的buffer
 * @param log           日志
 * @param f             测试的方法
 * @param test_info     测试的名称
 */
void data_self_test(io_buffer buff, logger log,
                    void (io_buffer::*f)(io_buffer &, size_t), const std::string &test_info) {
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> m(0, 0x10000);

    for (int i = 0; i < 100; i++) {
        /*_log.print("Before %s:buff begin=%zd,last=%zd,size=%zd", test_info.c_str(),
                  __test_get_begin(buff), __test_get_last(buff), buff.size());*/

        size_t pop_size = m(gen) % test_buff_size;
        (buff.*f)(buff, pop_size * sizeof(size_t));

        /*_log.print("%s %zd bytes!,%zd num", test_info.c_str(), pop_size * sizeof(size_t), pop_size);

        _log.print("After %s:buff begin=%zd,last=%zd,size=%zd\n", test_info.c_str(),
                  __test_get_begin(buff), __test_get_last(buff), buff.size());*/

    }
}


bool __test_other(std::array<io_buffer, array_size> &buff_array,
                  size_t start, size_t size, void (io_buffer::*f)(io_buffer &, size_t),
                  const std::string &test_info, logger log, bool incr) {
    std::vector<std::thread> threads;

    log.info("Start test,%s!", test_info.c_str());
    for (size_t i = 0; i < 1000; i++) {
        threads.emplace_back([=]() {
            for (size_t i = 0; i < 4; i++) {
                data_other_test(buff_array, log, f, incr, test_info);
            }
        });
    }

    for (std::thread &t:threads) {
        t.join();
    }

    log.info("Test end,start check!");

    size_t num = -1;
    bool is_start = true;

    for (size_t k = 0; k < array_size; k++) {
        io_buffer &buffer = buff_array[k];

        if (buffer.size() == 0) {
            continue;
        }

        if (buffer.size() % sizeof(size_t) != 0) {
            log.error("Unexpected behavior,buffer size not correct with %zd!", buffer.size());
            return false;
        }

        for (size_t index = 0; index < (buffer.size() / sizeof(size_t)); index++) {
            size_t j = __test_get_with_index(buffer, index);

            if (!(is_start || (j == num + 1) || (num == (start + size - 1) && j == start))) {
                log.error("Unexpected behavior,the %zdth buffer,num=%d,j=%d,index=%zd!", k, num, j, index);
                //__test_print(buffer);
                return false;
            }
            is_start = false;
            num = j;
        }
    }
    return true;
}


bool __test_self(io_buffer buff, size_t start, size_t size,
                 void (io_buffer::*f)(io_buffer &, size_t), const std::string &test_info,
                 logger log) {
    std::vector<std::thread> threads;

    log.info("Start test,%s!", test_info.c_str());
    for (size_t i = 0; i < 1000; i++) {
        threads.emplace_back([=]() {
            for (size_t i = 0; i < 4; i++) {
                data_self_test(buff, log, f, test_info);
            }
        });
    }

    for (std::thread &t:threads) {
        t.join();
    }

    log.info("Test end,start check!");

    size_t num = -1;
    bool is_start = true;


    if (buff.size() == 0 || buff.size() % sizeof(size_t) != 0 || buff.size() != size * sizeof(size_t)) {
        log.error("Unexpected behavior with %s,buffer size not correct with %zd!", test_info.c_str(),
                  buff.size());
        return false;
    }

    for (size_t index = 0; index < (buff.size() / sizeof(size_t)); index++) {
        size_t j = __test_get_with_index(buff, index);

        if (!(is_start || (j == num + 1) || (num == (start + size - 1) && j == start))) {
            log.info("Unexpected behavior with %s,num=%d,j=%d,index=%zd!", test_info.c_str(), num, j, index);
            //__test_print(buffer);
            return false;
        }
        is_start = false;
        num = j;
    }

    return true;
}


void other_test() {
    std::array<io_buffer, array_size> buff_array;

    logger log("IoBufferTestLog", logger::INFO);

    const size_t start = 0;
    const size_t size = 0x10000000;

    {
        char *temp = __test_get_num_ptr(start, size);
        log.info("Before push_back");
        buff_array[0].push_back_n(temp, size * sizeof(size_t));
        log.info("After push_back");
        delete temp;
    }

    if (!__test_other(buff_array, start, size, &io_buffer::pop_front_to_other_back_n,
                      "pop_front_to_other_back", log, false)) {
        std::cout << "pop_front_to_other_back method have bugs!" << std::endl;
    }

    if (!__test_other(buff_array, start, size, &io_buffer::pop_back_to_other_front_n,
                      "pop_back_to_other_front", log, true)) {
        std::cout << "pop_back_to_other_front method have bugs!" << std::endl;
    }
}


void self_test() {
    io_buffer buff;

    logger log("IoBufferTestLog",logger::INFO);

    const size_t start = 0;
    const size_t size = 0x10000000;

    {
        char *temp = __test_get_num_ptr(start, size);
        log.info("Before push_back");
        buff.push_back_n(temp, size * sizeof(size_t));
        log.info("After push_back");
        delete []temp;
    }

    if (!__test_self(buff, start, size, &io_buffer::pop_front_to_other_back_n,
                     "pop_front_to_self_back", log)) {
        std::cout << "pop_front_to_self_back method have bugs!" << std::endl;
    }

    if (!__test_self(buff, start, size, &io_buffer::pop_back_to_other_front_n,
                     "pop_back_to_self_front", log)) {
        std::cout << "pop_back_to_self_front method have bugs!" << std::endl;
    }
}


int main() {
    self_test();
}