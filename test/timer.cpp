//
// Created by suhang on 23-5-14.
//
#include <timer.h>

timer::timer(std::string &&ev_name) :
        event_name(std::move(ev_name)), start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch())) {}

timer::timer(const std::string &ev_name) :
        event_name(ev_name), start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch())) {}

timer::~timer() {
    std::chrono::microseconds end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    std::cout << "Time cost for " << event_name << ":" <<
              end.count() - start.count() << std::endl;
}