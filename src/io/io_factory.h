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


class io_factory {
public:
    io_factory(event_processor *manager) {
        this->ev_processor = manager;
    }

    template<typename _Tp, typename... _Args>
    std::future<std::shared_ptr<_Tp>> create_io_async(_Args &&...);

    template<typename _Tp, typename... _Args>
    std::shared_ptr<_Tp> create_io(_Args &&...);

    template<typename... _Args>
    std::future<std::shared_ptr<tcp>> create_tcp_async(const char *addr, unsigned short int port,_Args &&... args);

private:

    template<typename _Tp, typename... _Args>
    std::shared_ptr<_Tp> create_io_in_eventloop(_Args &&... args);

    event_processor *ev_processor;
    std::promise<std::shared_ptr<tcp>> tcp_indication;
};


template<typename _Tp, typename... _Args>
std::shared_ptr<_Tp> io_factory::create_io_in_eventloop(_Args &&... __args) {
    std::shared_ptr<_Tp> io_ptr = std::make_shared<_Tp>(io::this_is_private(0),std::forward<_Args>(__args)...);
    if (ev_processor->add_io(std::dynamic_pointer_cast<io>(io_ptr))) {
        return io_ptr;
    }

    io_ptr->set_valid(false);
    return std::shared_ptr<_Tp>();
}



template<typename _Tp, typename... _Args>
std::future<std::shared_ptr<_Tp>>
io_factory::create_io_async(_Args &&... __args) {

    std::shared_ptr<std::packaged_task<std::shared_ptr<_Tp>()>> run_task =
            std::make_shared<std::packaged_task<std::shared_ptr<_Tp>()>>(
                    [=]() mutable -> std::shared_ptr<_Tp> {
                        return create_io_in_eventloop<_Tp>(std::forward<_Args>(__args)...);
                    });

    std::future<std::shared_ptr<_Tp>> future = run_task->get_future();

    std::function<void()> task([=]() mutable {
        (*run_task)();
    });

    ev_processor->add_task(task);

    return future;
}

template<typename _Tp, typename... _Args>
std::shared_ptr<_Tp> io_factory::create_io(_Args &&... __args) {
    if (this->ev_processor->id() == std::this_thread::get_id()) {
        return create_io_in_eventloop<_Tp>(std::forward<_Args>(__args)...);
    } else {
        std::future<std::shared_ptr<_Tp>> io_future = create_io_async<_Tp>(std::forward<_Args>(__args)...);
        return io_future.get();
    }
}

template<typename... _Args>
std::future<std::shared_ptr<tcp>>
io_factory::create_tcp_async(const char *addr, unsigned short int port,_Args &&... args){
    std::promise<std::shared_ptr<tcp>> tcp_instance( std::make_shared<tcp>
            (io::this_is_private(0),std::forward<_Args>(__args)...));
    std::function<void()> task([=]) mutable {
        tcp_instance->connect();
        if (tcp_instance->tcp_status == 2)
            tcp_indication.set_value(std::move(tcp_instance));
        else
            tcp_indication.set_exception(std::current_exception());
    } 
    ev_processor->add_task(task)
    std::future<std::shared_ptr<tcp>> future = prom->get_future;
    return future;
}

#endif //NETPROCESSOR_IO_FACTORY_H
