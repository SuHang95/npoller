//
// Created by suhang on 19-5-13.
//

#include<sulog/simple_logger.h>
#include<iostream>
#include<chrono>

using namespace std;
using namespace std::chrono;

class Timer {
public:
    Timer() : m_begin(high_resolution_clock::now()) {}

    void reset() { m_begin = high_resolution_clock::now(); }

    //默认输出毫秒
    int64_t elapsed() const {
        return duration_cast<chrono::milliseconds>(high_resolution_clock::now() - m_begin).count();
    }

    //微秒
    int64_t elapsed_micro() const {
        return duration_cast<chrono::microseconds>(high_resolution_clock::now() - m_begin).count();
    }

    //纳秒
    int64_t elapsed_nano() const {
        return duration_cast<chrono::nanoseconds>(high_resolution_clock::now() - m_begin).count();
    }

    //秒
    int64_t elapsed_seconds() const {
        return duration_cast<chrono::seconds>(high_resolution_clock::now() - m_begin).count();
    }

    //分
    int64_t elapsed_minutes() const {
        return duration_cast<chrono::minutes>(high_resolution_clock::now() - m_begin).count();
    }

    //时
    int64_t elapsed_hours() const {
        return duration_cast<chrono::hours>(high_resolution_clock::now() - m_begin).count();
    }

private:
    time_point<high_resolution_clock> m_begin;
};

int main() {
    simple_logger simpleLogger("test", base_logger::DEBUG, minute);

    Timer t1;
    for (int i = 0; i < 0x1000000; i++) {
        simpleLogger.debug("Hello,baseLogger!This is %d loop!%s", i,"Great haha!");
    }
    cout << "Debug level use " << t1.elapsed() << endl;


    Timer t2;
    for (int i = 0; i < 0x1000000; i++) {
        simpleLogger.info("Hello,baseLogger!This is %d loop!%s", i,"Great haha!");
    }
    cout << "Info level use " << t2.elapsed() << endl;

    return 0;
}