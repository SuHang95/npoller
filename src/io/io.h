//
// Created by suhang on 2020/1/26.
//

#ifndef NETPROCESSOR_IO_H
#define NETPROCESSOR_IO_H

#include <sulog/logger.h>
#include "task.h"

#include <sys/epoll.h>
#include <tbb/concurrent_queue.h>


class io_factory;

class event_processor;

const int cache_line_size = 64;

//the file type used by this class must support epoll and non-blocking
class io {
protected:
    struct this_is_private {
        explicit inline this_is_private(int d) {}
    };

public:
    struct io_type {
        unsigned readable:1;
        unsigned writable:1;
        unsigned support_epollrdhup:1;
    };

    //the protected method is only used on event loop thread
    io(const this_is_private &, const int _fd, const io_type, const logger &_log);

    io(const this_is_private &, const int _fd, const io_type type) : io(this_is_private(0), _fd, type,
                                                                        logger("io" + std::hash<std::string>{}(
                                                                                std::to_string(fd +
                                                                                               reinterpret_cast<size_t>(this))),
                                                                               logger::INFO)) {}

    io(const io &) = delete;

    io &operator=(const io &) = delete;

    io(io &&) = delete;

    io &operator=(io &&) = delete;

    virtual ~io();

    inline int id() const {
        return std::hash<std::string>{}(
                std::to_string(fd + reinterpret_cast<size_t>(this)));
    }

    //regist the event
    virtual bool regist(const std::shared_ptr<io_op> &);

protected:
    io() {}

    io(const logger &_log) : log(_log) {}

    virtual void direct_read();

    virtual void direct_write();

    virtual bool process_event(::epoll_event &);

    virtual bool get_event(epoll_event &ev);

    void set_manager(event_processor *__manager);

    virtual void process_read();

    virtual void clean_read();

    virtual void write_flush();

    virtual void clean_write();

    virtual void process_write();

    //status reader
    event_processor *get_manager_unsafe();

    //these unsafe load method was load the value which not used with atomic operation,
    //these methods must only used in the method which only run on the thread manager's event loop run on
    virtual bool valid_unsafe();

    virtual bool readable_unsafe();

    virtual bool writable_unsafe();

    virtual bool write_busy_unsafe();

    event_processor *get_manager_safe();

    bool valid_safe();

    bool readable_safe();

    bool writable_safe();

    bool write_busy_safe();

    virtual void set_valid(bool);

    virtual void set_readable(bool);

    virtual void set_writable(bool);

    virtual void set_write_busy(bool);

    virtual void mark_invalid();

    inline bool compare_status(unsigned char _read, unsigned char _write,
                               unsigned _support_epollrdhup, unsigned char _busy) {
        return ((_read != readable_unsafe()) || (_write != writable_unsafe()) ||
                (_support_epollrdhup) != support_epollrdhup || (_busy != write_busy_unsafe()));
    }


    //these fields maybe used in the thread different with manager's thread which event loop on,
    //so we used atomic value
    std::atomic<event_processor *> manager;
    //status descriptor
    std::atomic<bool> valid;
    std::atomic<bool> readable;
    std::atomic<bool> writable;
    std::atomic<bool> write_busy;
    //file descriptor
    int fd;
    bool support_epollrdhup;

    uint8_t cache_line_padding1[cache_line_size - 4 * sizeof(std::atomic<unsigned char>)
                                - sizeof(std::atomic<event_processor *>) - sizeof(int)];


    std::size_t have_write;


    logger log;
    io_buffer read_buffer;
    io_buffer write_buffer;

    std::shared_ptr<io_op> temp_read_event;

    tbb::concurrent_queue<std::shared_ptr<io_op>> read_list;
    tbb::concurrent_queue<std::shared_ptr<io_op>> write_list;

    std::deque<std::shared_ptr<io_op>> wait_write_list;

    friend class event_processor;

    friend class io_factory;

    template<typename _Tp, typename... _Args>
    friend std::shared_ptr<_Tp> std::make_shared(_Args &&...);

    static const int max_retry_times;
};

inline event_processor *io::get_manager_unsafe() {
    return *reinterpret_cast<event_processor **>(&manager);
}

inline bool io::valid_unsafe() {
    return *(reinterpret_cast<bool *>(&valid));
}


inline bool io::readable_unsafe() {
    return *(reinterpret_cast<bool *>(&readable));
}


inline bool io::writable_unsafe() {
    return *(reinterpret_cast<bool *>(&writable));
}


inline bool io::write_busy_unsafe() {
    return *(reinterpret_cast<bool *>(&write_busy));
}

inline event_processor *io::get_manager_safe() {
    return manager.load(std::memory_order_relaxed);
}


inline bool io::valid_safe() {
    return valid.load(std::memory_order_relaxed);
}

inline bool io::readable_safe() {
    return readable.load(std::memory_order_relaxed);
}

inline bool io::writable_safe() {
    return writable.load(std::memory_order_relaxed);
}

inline bool io::write_busy_safe() {
    return write_busy.load(std::memory_order_relaxed);
}

inline void io::set_valid(bool _valid) {
    valid.store(_valid, std::memory_order_relaxed);
}


inline void io::set_readable(bool _readable) {
    readable.store(_readable, std::memory_order_relaxed);
}

inline void io::set_writable(bool _writable) {
    writable.store(_writable, std::memory_order_relaxed);
}

inline void io::set_write_busy(bool _write_busy) {
    write_busy.store(_write_busy, std::memory_order_relaxed);
}


#endif //NETPROCESSOR_IO_H
