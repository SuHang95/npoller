//
// Created by suhang on 2020/1/26.
//

#ifndef IO_H
#define IO_H

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
    };

    constexpr static io_type readable = {1,0};
    constexpr static io_type writable = {0,1};

    //the protected method is only used on event loop thread
    io(const this_is_private &, const int _fd, const io_type,
            const bool _support_epollrdhup, const logger &_log);

    io(const this_is_private &, const int _fd, const io_type type,
            const bool _support_epollrdhup) : io(this_is_private(0), _fd, type,_support_epollrdhup,
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

    //register the event
    virtual bool do_register(const std::shared_ptr<io_op> &);

protected:
    io() {}

    io(const logger &_log) : log(_log) {}

    virtual void direct_read();

    virtual void direct_write();

    virtual bool process_event(::epoll_event &);

    virtual bool get_event(epoll_event &ev);

    void update_manager(event_processor *__manager);

    virtual void process_read();

    virtual void clean_read();

    virtual void write_flush();

    virtual void clean_write();

    virtual void process_write();

    //status reader
    event_processor *get_manager_unsafe();

    //these unsafe load method was load the value which not used with atomic operation,
    //these methods must only used in the method which only run on the thread manager's event loop run on
    bool valid_unsafe();

    bool readable_unsafe();

    bool writable_unsafe();

    bool write_busy_unsafe();

    event_processor *get_manager_safe();

    bool valid_safe();

    bool readable_safe();

    bool writable_safe();

    bool write_busy_safe();

    void set_valid(bool);

    void set_readable(bool);

    void set_writable(bool);

    void set_write_busy(bool);

    void mark_invalid();

    inline bool compare_status(unsigned char _read, unsigned char _write,
                               unsigned _support_epollrdhup, unsigned char _busy) {
        return ((_read != readable_unsafe()) || (_write != writable_unsafe()) ||
                (_support_epollrdhup) != _support_epollrdhup || (_busy != write_busy_unsafe()));
    }


    //these fields maybe used in the thread different with manager's thread which event loop on,
    //so we used atomic value
    std::atomic<event_processor *> manager;
    //status descriptor
    std::atomic<bool> valid;
    std::atomic<io_type> type;
    std::atomic<bool> write_busy;
    //file descriptor
    int fd;
    bool support_epollrdhup;

    uint8_t cache_line_padding1[cache_line_size - 3 * sizeof(std::atomic<unsigned char>)
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
    return (reinterpret_cast<io_type *>(&type)->readable);
}


inline bool io::writable_unsafe() {
    return (reinterpret_cast<io_type *>(&type)->writable);
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
    return type.load(std::memory_order_relaxed).readable;
}

inline bool io::writable_safe() {
    return type.load(std::memory_order_relaxed).writable;
}

inline bool io::write_busy_safe() {
    return write_busy.load(std::memory_order_relaxed);
}

inline void io::set_valid(bool _valid) {
    valid.store(_valid, std::memory_order_relaxed);
}


inline void io::set_readable(bool _readable) {
    io_type expected_type = type.load(std::memory_order_relaxed);
    io_type current_type;
    bool success;
    do {
        current_type = expected_type;
        if (((current_type.readable == 0x01) & _readable) ||
            ((current_type.readable == 0x0) & !_readable)) {
            return;
        }
        current_type.readable = _readable ? 0x01 : 0;
        success = type.compare_exchange_strong(expected_type,current_type,std::memory_order_relaxed
                ,std::memory_order_relaxed);
    } while (!success);
}

inline void io::set_writable(bool _writable){
    io_type expected_type = type.load(std::memory_order_relaxed);
    io_type current_type;
    bool success;
    do {
        current_type = expected_type;
        if (((current_type.writable== 0x01) & _writable) ||
            ((current_type.writable == 0x0) & !_writable)) {
            return;
        }
        current_type.writable = _writable ? 0x01 : 0;
        success = type.compare_exchange_strong(expected_type,current_type,std::memory_order_relaxed
                ,std::memory_order_relaxed);
    } while (!success);
}

inline void io::set_write_busy(bool _write_busy) {
    write_busy.store(_write_busy, std::memory_order_relaxed);
}


#endif //IO_H
