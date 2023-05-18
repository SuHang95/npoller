//
// Created by suhang on 23-5-16.
//

#ifndef NPOLLER_IO_AWAITABLE_H
#define NPOLLER_IO_AWAITABLE_H

#include <coroutine>

template <typename T>
class io_awaitable {
    void await_suspend(std::coroutine_handle<> h){

    }
    bool await_ready(){

    }
    void await_resume() {}
};


#endif //NPOLLER_IO_AWAITABLE_H
