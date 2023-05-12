//
// Created by suhang on 2020/1/27.
//

#ifndef NETPROCESSOR_IO_FACTORY_H
#define NETPROCESSOR_IO_FACTORY_H

#include <future>
#include <sulog/logger.h>
#include "io.h"
#include "tcp.h"
#include "event_processor.h"
#include "exception"
#include "tcp_acceptor.h"


class io_factory {
public:
    io_factory(event_processor *manager) {
        this->ev_processor = manager;
    }

    template<typename _Tp, typename... _Args>
    std::future<std::shared_ptr<_Tp>> create_io_async(_Args &&...);

    template<typename _Tp, typename... _Args>
    std::shared_ptr<_Tp> create_io(_Args &&...);

    template<typename _Tp, typename... _Args>
    void create_io_with_callback(std::function<void(std::shared_ptr<_Tp>)> &callback, _Args &&...);

    template<typename... _Args>
    std::future<std::shared_ptr<tcp>> create_tcp_async(const char *addr, unsigned short int port, _Args &&... args);

private:

    template<typename _Tp, typename... _Args>
    std::shared_ptr<_Tp> create_io_in_eventloop(_Args &&... args);

    template<typename _Tp, typename... _Args>
    requires std::same_as<_Tp, tcp_acceptor>
    std::shared_ptr<_Tp> create_io_in_eventloop(_Args &&... __args);

    event_processor *ev_processor;
};


template<typename _Tp, typename... _Args>
std::shared_ptr<_Tp> io_factory::create_io_in_eventloop(_Args &&... __args) {
    std::shared_ptr<_Tp> io_ptr = std::make_shared<_Tp>(io::this_is_private(0), std::forward<_Args>(__args)...);

    if (io_ptr == nullptr) {
        return std::shared_ptr<_Tp>();
    }

    if (ev_processor->add_io(std::dynamic_pointer_cast<io>(io_ptr))) {
        return io_ptr;
    }

    io_ptr->mark_invalid();
    return std::shared_ptr<_Tp>();
}

template<typename _Tp, typename... _Args>
requires std::same_as<_Tp, tcp_acceptor>
std::shared_ptr<_Tp> io_factory::create_io_in_eventloop(_Args &&... __args) {
    std::shared_ptr<tcp_acceptor> io_ptr = std::make_shared<tcp_acceptor>(io::this_is_private(0),
                                                                          std::forward<_Args>(__args)...);

    if (io_ptr == nullptr) {
        return std::shared_ptr<tcp_acceptor>();
    }

    if (io_ptr->start_accept(ev_processor)) {
        return std::shared_ptr<tcp_acceptor>();
    }

    return io_ptr;
}

template<typename _Tp, typename... _Args>
std::future<std::shared_ptr<_Tp>>
io_factory::create_io_async(_Args &&... __args) {

    std::shared_ptr<std::packaged_task<std::shared_ptr<_Tp>()>> run_task =
            std::make_shared<std::packaged_task<std::shared_ptr<_Tp>()>>(
                    [this, ...args = (std::forward<_Args>(__args))]() mutable -> std::shared_ptr<_Tp> {
                        return create_io_in_eventloop<_Tp>(std::forward<_Args>(args)...);
                    });

    std::future<std::shared_ptr<_Tp>> future = run_task->get_future();

    std::function<void()> task([=]() mutable {
        (*run_task)();
    });

    ev_processor->add_task(task);

    return future;
}


template<typename _Tp, typename... _Args>
void io_factory::create_io_with_callback(std::function<void(std::shared_ptr<_Tp>)> &callback, _Args &&... __args) {
    std::function<void()> task([this, callback, ...args = (std::forward<_Args>(__args))]() mutable {
        std::shared_ptr<_Tp> ptr = create_io_in_eventloop<_Tp>(std::forward<_Args>(args)...);
        callback(ptr);
    });

    ev_processor->add_task(task);
}

template<typename _Tp, typename... _Args>
std::shared_ptr<_Tp> io_factory::create_io(_Args &&... __args) {
    if (this->ev_processor->thread_id() == std::this_thread::get_id()) {
        return create_io_in_eventloop<_Tp>(std::forward<_Args>(__args)...);
    } else {
        std::future<std::shared_ptr<_Tp>> io_future = create_io_async<_Tp>(std::forward<_Args>(__args)...);
        return io_future.get();
    }
}

template<typename... _Args>
std::future<std::shared_ptr<tcp>>
io_factory::create_tcp_async(const char *addr, unsigned short int port, _Args &&... args) {
    std::promise<std::shared_ptr<tcp>> connect_promise;

    //this future is for connected_handler use, not return value
    std::future<std::shared_ptr<tcp>> connect_future = connect_promise.get_future();

    //we want this future return when connect success, tcp instance initialization and connect call return will not
    //return the future.

    //We need this handler ran after tcp connected, related code is in tcp.cpp
    std::packaged_task<std::shared_ptr<tcp>(bool)> *connected_handler =
            new std::packaged_task<std::shared_ptr<tcp>(bool)>(
                    [_connect_future= std::move(connect_future)](bool successful) mutable -> std::shared_ptr<tcp> {
                        return successful ? _connect_future.get() : std::shared_ptr<tcp>();
                    }
            );

    //
    std::future<std::shared_ptr<tcp>> future = connected_handler->get_future();

    std::shared_ptr<std::packaged_task<void()>> connect_task =
            std::make_shared<std::packaged_task<void()>>(
                    [this, addr, port, connected_handler, _connect_promise = std::move(connect_promise),
                            ..._args = std::forward<_Args>(args)]() mutable {
                        std::shared_ptr<tcp> tcp_instance = create_io_in_eventloop<tcp,_Args...>(std::forward<_Args>(_args)...);
                        _connect_promise.set_value(tcp_instance);
                        //this handler must not shared_ptr,otherwise it will cause circular reference
                        tcp_instance->set_connected_handler(connected_handler);
                        tcp_instance->connect(addr, port);
                    });

    std::function<void()> task([=]() mutable {
        (*connect_task)();
    });

    ev_processor->add_task(task);

    return future;
}

#endif //NETPROCESSOR_IO_FACTORY_H
