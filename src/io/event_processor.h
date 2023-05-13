#ifndef __event_processor_h__
#define __event_processor_h__

#include<cstring>
#include<exception>
#include<memory>
#include<mutex>
#include<iostream>
#include<list>
#include<map>
#include<utility>

#include<tbb/concurrent_hash_map.h>
#include<tbb/concurrent_queue.h>

/*linux net header*/
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/un.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<functional>
#include<future>

#include <sulog/logger.h>
#include "task.h"
#include "internal_task.h"
#include <iobuffer/io_buffer.h>

class io_factory;

class io;

class event_processor {
public:
    event_processor() noexcept;

    event_processor(const logger &__log) noexcept;

    ~event_processor();

    event_processor(const event_processor &) = delete;

    event_processor &operator=(const event_processor &) = delete;

    event_processor(event_processor &&) = delete;

    event_processor &operator=(event_processor &&) = delete;

    bool add_io(std::shared_ptr<io>) throw();

    inline bool remove_io(std::shared_ptr<io> ptr);

    bool remove_io(int fd);

    bool add_write_instant_task(int fd);

    size_t id() const {
        return reinterpret_cast<size_t>(this);
    }

    void add_task(const std::function<void()> &task);

    logger get_logger();

    io_factory get_factory();

    std::thread::id thread_id() const;

protected:
    void init();

    void process();

    void close();

    void notify_event();

    //file descriptor
    const int epfd;

    //when the we need the epoll_wait return,the pipe_fd will be used
    int pipe_fd[2];

    logger log;

    void process_instant_write();
    
    void process_task_queue(int&);

    enum status_code : unsigned char {
        initializing = 0,
        initialized = 1,
        invalid = 2,
        working = 3,
        ready_waiting = 4,
        waiting = 5,
        ready_close = 6,
        stop_work = 7,
        closed = 8,
    };

    //-1 means uninitialized.0 means not valid,1 means valid
    std::atomic<status_code> status;

    tbb::concurrent_hash_map<int, std::shared_ptr<io>> io_map;

    //some io doesn't need to wait EPOLLOUT event,because the
    //their io_buffer in kernel may not full,so they may won't waked
    //up by epoll_wait,but we need wake them up because we need to
    //process the write event in them soon,so use this list to do so


    //every time we use it,we will process the io event and erase the
    //item in it
    tbb::concurrent_queue<std::shared_ptr<io>> ready_to_write;

    std::thread worker;

    std::atomic<std::thread::id> worker_id;

    //used only for init and none-synchronized api
    tbb::concurrent_queue<std::function<void()>> task_list;

    std::deque<std::pair<std::function<void()>,int>> local_task_list;

    epoll_event *event_buff;

    std::atomic<size_t> task_index;

    enum signal : unsigned char {
        close_signal = 0,
        task_signal = 1,
    };

    const static int max_events;

    friend class io_factory;
};

inline std::thread::id event_processor::thread_id() const {
    return worker_id.load(std::memory_order_relaxed);
}

inline logger event_processor::get_logger() {
    return log;
}


#endif
