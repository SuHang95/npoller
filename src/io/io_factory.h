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
    void create_io_with_callback(std::packaged_task<void(std::shared_ptr<_Tp> &&, const std::string &)> &&callback,
                                 _Args &&...);

    template<typename _Tp, typename... _Args>
    requires std::same_as<_Tp, tcp>
    std::future<std::shared_ptr<_Tp>> create_io_async(const char *addr, unsigned short int port, _Args &&... args);

    template<typename _Tp, typename... _Args>
    requires std::same_as<_Tp, tcp>
    void create_io_with_callback(std::packaged_task<void(std::shared_ptr<_Tp> &&, const std::string &)> &&callback,
                                 const char *addr, unsigned short int port, _Args &&...args);

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

    if (io_ptr->is_open() &&
        ev_processor->add_io(std::dynamic_pointer_cast<io>(io_ptr))) {
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

    if (!io_ptr->start_accept(ev_processor)) {
        return std::shared_ptr<tcp_acceptor>();
    }

    return io_ptr;
}

template<typename _Tp, typename... _Args>
std::future<std::shared_ptr<_Tp>>
io_factory::create_io_async(_Args &&... __args) {
    std::promise<std::shared_ptr<_Tp>> creation_promise;

    std::future<std::shared_ptr<_Tp>> future = creation_promise.get_future();
    std::packaged_task<void()>
            run_task(
            [this, promise = std::move(creation_promise), ...args = (std::forward<_Args>(
                    __args))]() mutable {
                try {
                    promise.set_value(create_io_in_eventloop<_Tp>(std::forward<_Args>(args)...));
                } catch (std::exception &e) {
                    promise.set_exception(std::current_exception());
                }
            });


    ev_processor->add_task(std::move(run_task));

    return std::move(future);
}


template<typename _Tp, typename... _Args>
void
io_factory::create_io_with_callback(std::packaged_task<void(std::shared_ptr<_Tp> &&, const std::string &)> &&callback,
                                    _Args &&... __args) {
    std::packaged_task<void()> task(
            [this, _callback = std::move(callback), ...args = (std::forward<_Args>(__args))]() mutable {
                try {
                    std::shared_ptr<_Tp> ptr = create_io_in_eventloop<_Tp>(std::forward<_Args>(args)...);
                    if (ptr != nullptr) {
                        _callback(std::move(ptr), std::string{});
                    } else {
                        _callback(nullptr, strerror(errno));
                    }
                } catch (std::exception &e) {
                    _callback(nullptr, e.what());
                }
            });

    ev_processor->add_task(std::move(task));
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


template<typename _Tp, typename... _Args>
requires std::same_as<_Tp, tcp>
void
io_factory::create_io_with_callback(std::packaged_task<void(std::shared_ptr<_Tp> &&, const std::string &)> &&callback,
                                    const char *addr, unsigned short int port, _Args &&...args) {
    std::packaged_task<void()> connect_task(
            [this, addr, port, _callback = std::move(callback),
                    ..._args = std::forward<_Args>(args)]() mutable {
                std::shared_ptr<_Tp> instance = create_io_in_eventloop<_Tp, _Args...>(
                        std::forward<_Args>(_args)...);
                if (instance != nullptr && instance->is_open()) {
                    //this handler must not shared_ptr,otherwise it will cause circular reference
                    instance->set_connect_callback(std::move(_callback));
                    instance->connect(addr, port);
                } else {
                    _callback(nullptr, strerror(errno));
                }
            });
    ev_processor->add_task(std::move(connect_task));
}

template<typename _Tp, typename... _Args>
requires std::same_as<_Tp, tcp>
std::future<std::shared_ptr<_Tp>>
io_factory::create_io_async(const char *addr, unsigned short int port, _Args &&... args) {
    std::promise<std::shared_ptr<_Tp>> connect_promise;

    //this future is for connected_handler use, not return value
    std::future<std::shared_ptr<_Tp>> connect_future = connect_promise->get_future();

    //we want this future return when connect success, tcp instance initialization and connect call return will not
    //return the future.

    //We need this handler ran after tcp connected, related code is in tcp.cpp
    std::packaged_task<void(std::shared_ptr<_Tp> &&, const std::string &)> callback(
            [_promise = std::move(connect_promise)](std::shared_ptr<_Tp> &&ptr,
                                                    const std::string &error_message) mutable {
                if (ptr != nullptr) {
                    _promise->set_value(std::move(ptr));
                } else {
                    _promise->set_exception(
                            std::make_exception_ptr(std::runtime_error(error_message)));
                }
            }
    );

    this->create_io_with_callback<_Tp>(std::move(callback), addr, port,
                                       std::forward<_Args>(args)...);
    return connect_future;
}

#endif //NETPROCESSOR_IO_FACTORY_H
