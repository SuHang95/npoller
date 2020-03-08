//
// Created by suhang on 19-9-18.
//

#ifndef NETPROCESSOR_CONCURRENT_BUFFER_H
#define NETPROCESSOR_CONCURRENT_BUFFER_H

#include "buffer.h"
#include <deque>
#include <memory>
#include <shared_mutex>
#include <vector>


class concurrent_buffer final {
public:
    concurrent_buffer();

    concurrent_buffer(const concurrent_buffer &);

    concurrent_buffer &operator=(const concurrent_buffer &);

    size_t inline size();

    class r_context;

    class rw_context;


private:
    ~concurrent_buffer();

    class page;

    struct interval;

    struct interval_block;

    struct control_block {
        std::deque<std::shared_ptr<page>> buffer_;
        std::shared_mutex guard_mutex_;
        size_t begin;
        size_t end;
    };
};

struct concurrent_buffer::interval_block {
    std::array<concurrent_buffer::interval, 4> intervals;
    interval_block *next_;
};

struct interval {
    int start;
    int end;
    std::shared_mutex data_mutex_;

    unsigned int valid:1;
};


class concurrent_buffer::page {
public:
    page();


private:
    char *addr_;
    std::shared_mutex guard_mutex_;
};

#endif //NETPROCESSOR_CONCURRENT_BUFFER_H
