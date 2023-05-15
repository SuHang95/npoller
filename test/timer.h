//
// Created by suhang on 23-5-14.
//

#ifndef NPOLLER_TIMER_H
#define NPOLLER_TIMER_H
#include <string>
#include <chrono>
#include <iostream>

class timer{
public:
    timer(const std::string& ev_name);
    timer(std::string&& ev_name);
    ~timer();
private:
    std::string event_name;
    std::chrono::microseconds start;
};

#endif //NPOLLER_TIMER_H
