#include "event_processor.h"
#include "io.h"
#include "io_factory.h"
#include <bits/stdc++.h>

const int io::max_retry_times = 3;

const int event_processor::max_events = 1024;

const int wait_micro_second = 1000;


event_processor::event_processor() noexcept:
        event_processor(logger("IoProcessor#" + std::to_string(reinterpret_cast<size_t>(this)),
                               logger::INFO)) {}

event_processor::event_processor(const logger &__log) noexcept
        : log(__log), status(initializing), epfd(epoll_create1(0)),
          worker([this]() { this->init(); }), event_buff(nullptr) {

    if (epfd == -1) {
        status.store(invalid, std::memory_order_release);
        log.error("Create epoll fd fail:%s!", strerror(errno));
        return;
    }

    if (pipe2(pipe_fd, O_NONBLOCK) < 0) {
        log.error("Create pipe fd fail:%s!", strerror(errno));
        status.store(invalid, std::memory_order_release);
        ::close(epfd);
        return;
    }

    event_buff = new epoll_event[max_events];

    struct epoll_event ev;
    ev.data.fd = pipe_fd[0];
    ev.events = EPOLLIN;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd[0], &ev) == -1) {
        log.error("Add listen pipe fd error:%s!", strerror(errno));
    }

    status.store(initialized, std::memory_order_release);

    log.debug("Event processor create successful,id:%llu,thread_id:%llu,out pipe fd:%d,in pipe fd:%d,"
              "epoll fd:%d!", id(), std::hash<std::thread::id>()(thread_id()), pipe_fd[1], pipe_fd[0], epfd);

    while (status.load(std::memory_order_relaxed) <= invalid) {
        usleep(wait_micro_second);
    }
}

void event_processor::close() {

}


event_processor::~event_processor() {
    status_code temp_st = status.load(std::memory_order_relaxed);
    if (temp_st == stop_work || temp_st == closed) {
        delete[]event_buff;
        return;
    }

    log.debug("An Epoll instance thread_id:%llu will be destroyed!", id());
    status.store(ready_close, std::memory_order_relaxed);

    unsigned char temp[1] = {close_signal};
    if (write(pipe_fd[1], temp, 1) > 0) {
        worker.join();
    } else {
        //if the worker thread has not waken up,releasing io will complete in this thread
        log.warn("The worker thread is not waked up:%s!", strerror(errno));
        worker.~thread();
    }

    while (!status.load(std::memory_order_acquire) == stop_work) {
        usleep(wait_micro_second);
    }

    close();

    if (event_buff != nullptr)
        delete[]event_buff;
}

bool event_processor::add_io(std::shared_ptr<io> _io) throw() {
    event_processor::status_code status_local = status.load(std::memory_order_relaxed);
    if ((status_local != working && status_local != ready_waiting) || _io == nullptr ||
        !_io->valid_safe() || _io->fd == epfd) {

        log.warn("A file descriptor %d invalid or closed"
                 "was tried to add in an epoll instance!", _io->id());
        return false;
    }

    {
        tbb::concurrent_hash_map<int, std::shared_ptr<io>>::const_accessor accessor;
        if (io_map.find(accessor, _io->fd)) {
            throw std::runtime_error("Try to add a fd " + std::to_string(_io->fd) +
                                     " which in epoll fd 's map!");
        }
    }

    retry:

    int fd = _io->fd;
    struct epoll_event ev;
    if (!_io->get_event(ev)) {
        return false;
    }
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        if (errno == EBADF) {
            _io->mark_invalid();
            log.warn("An invalid file descriptor %d was"
                     "tried to add in an Epoll instance %d", fd, epfd);
            return false;
        } else if (errno == EINVAL) {
            if (_io->support_epollrdhup) {
                _io->support_epollrdhup = false;
                goto retry;
            }
            log.error("A file descriptor %d was tried to"
                      "add in an Epoll instance %d,some error occur!", fd, epfd);
            return false;
        } else if (errno == EEXIST) {
            log.error("A file descriptor %d was tried to"
                      "add in an Epoll instance %d,but has been added it before!", fd, epfd);
            throw std::runtime_error("Try to add a fd which epoll fd is listening!");
        } else if (errno == EPERM) {
            log.error("A file descriptor %d which dont"
                      "support epoll was tried to add in an Epoll instance %d!", fd, epfd);
        } else {
            log.error("A file descriptor %d which was"
                      "tried to add in an Epoll instance %d,some error occur,%s!",
                      fd, epfd, strerror(errno));
            return false;
        }
    } else {
        io_map.insert(std::make_pair(fd, _io));
        _io->update_manager(this);
        log.debug("Add a file descriptor %d in epoll instance %d", fd, epfd);
    }
    return true;
}

bool event_processor::add_write_instant_task(int fd) {
    std::shared_ptr<io> io_ptr;

    {
        tbb::concurrent_hash_map<int, std::shared_ptr<io>>::const_accessor accessor;
        if (!io_map.find(accessor, fd)) {
            return false;
        }
        io_ptr = accessor->second;
    }

    ready_to_write.push(std::move(io_ptr));

    notify_event();

    return true;
}


bool event_processor::remove_io(std::shared_ptr<io> it) {
    return remove_io(it->fd);
}


bool event_processor::remove_io(int fd) {

    if (!io_map.erase(fd)) {
        log.warn("Try to delete an io instance %d which does not exist from %d!", fd, epfd);
        return false;
    }

    log.debug("Delete a file descriptor %d from epoll instance %d", fd, epfd);


    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        log.error("Try to delete an io instance %d from "\
                "epoll instance %d,some error occur!", fd, epfd);
        return false;
    }

    return true;
}

void event_processor::init() {
    status_code _status = initialized;
    //wait for epfd and pipe fd init
    while (!status.compare_exchange_strong(_status, working,
                                           std::memory_order_acquire, std::memory_order_relaxed)) {

        if (_status == invalid) {
            return;
        }
        usleep(1000);
        _status = initialized;
    }
    worker_id.store(std::this_thread::get_id(), std::memory_order_relaxed);

    log.debug("The event loop of event processor:%llu start!", id());

    process();
}


void event_processor::process_instant_write() {
    while (!ready_to_write.empty()) {
        std::shared_ptr<io> io_ptr;
        if (!ready_to_write.try_pop(io_ptr)) {
            continue;
        }

        epoll_event ev;
        unsigned char __readable = io_ptr->readable_unsafe();
        unsigned char __writable = io_ptr->writable_unsafe();
        unsigned char __support_epollrdhup = io_ptr->support_epollrdhup;
        unsigned char __write_busy = io_ptr->write_busy_unsafe();

        io_ptr->write_flush();
        io_ptr->direct_write();
        io_ptr->process_write();

        if (io_ptr->get_event(ev)) {
            if (io_ptr->compare_status(__readable, __writable, __support_epollrdhup, __write_busy)) {
                if (epoll_ctl(epfd, EPOLL_CTL_MOD, io_ptr->fd, &ev) == -1) {
                    log.error("A file descriptor %d was tried to"\
                        " mod in an Epoll instance %d,some error occur!", io_ptr->fd, epfd);

                }
            }
        }
    }
}


void event_processor::process() {
    bool close_flag = false;
    int task_index = 0;

    while (1) {

        status.store(ready_waiting, std::memory_order_relaxed);

        process_task_queue(task_index);

        process_instant_write();
        //detect if we need close this event loop
        if (close_flag) {
            status.store(stop_work, std::memory_order_release);
            return;
        }

        status.store(waiting, std::memory_order_relaxed);

        int fd_nums = epoll_wait(epfd, event_buff, max_events, -1);

        status.store(working, std::memory_order_relaxed);

        log.debug("The event processor:%llu wake up,event nums:%d!", id(), fd_nums);

        if (fd_nums < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                log.error("An epoll_wait operation fail :%s!", strerror(errno));
                return;
            }
        }
        for (int i = 0; i < fd_nums; i++) {
            if (event_buff[i].data.fd == pipe_fd[0]) {
                char temp[1];
                if (read(pipe_fd[0], temp, 1) <= 0) {
                    log.error("Read notify pipe error %s!", strerror(errno));
                }
                if (status.load(std::memory_order_relaxed) == ready_close)
                    close_flag = true;
                continue;
            }

            std::shared_ptr<io> io_ptr;
            bool __readable, __writable, __support_epollrdhup, __write_busy;
            {
                tbb::concurrent_hash_map<int, std::shared_ptr<io>>::const_accessor accessor;
                if (!io_map.find(accessor, event_buff[i].data.fd)) {
                    log.warn("Can't find %d from the io_cache associated", \
                        "with epoll instance %d", (event_buff[i].data.fd),
                             epfd);
                    continue;
                }
                if (log.is_debug_enable()) {
                    uint8_t in = event_buff[i].events & EPOLLIN;
                    uint8_t out = event_buff[i].events & EPOLLOUT;
                    uint8_t rdhup = event_buff[i].events & EPOLLRDHUP;
                    log.debug("Some event occurred on descriptor %d,in event:%d,out event:%d,rdhup event:%d",
                              (event_buff[i].data.fd),
                              in, out, rdhup);
                }
                io_ptr = accessor->second;
            }
            //to get some status such that we can avoid some unnecessary epoll_ctl modify!
            __readable = io_ptr->readable_unsafe();
            __writable = io_ptr->writable_unsafe();
            __support_epollrdhup = io_ptr->support_epollrdhup;
            __write_busy = io_ptr->write_busy_unsafe();

            if (io_ptr->process_event(event_buff[i])) {
                //if the io instance' state was changed,we will use epoll_ctl to modify its status
                if (io_ptr->compare_status(__readable, __writable, __support_epollrdhup, __write_busy)) {
                    if (epoll_ctl(epfd, EPOLL_CTL_MOD, io_ptr->fd, &event_buff[i]) == -1) {
                        log.error("A file descriptor %d was tried to", \
                            " mod in an Epoll instance %d,some error occur %s!",
                                  io_ptr->fd, epfd, strerror(errno));
                    }
                }
            }
        }
    }
}


void event_processor::add_task(const std::function<void()> &task) {
    if (thread_id() != std::this_thread::get_id()) {
        task_index.fetch_add(1);
        task_list.push(task);
        if (this->status.load(std::memory_order_relaxed) != working) {
            unsigned char temp[1] = {task_signal};
            if (write(pipe_fd[1], temp, 1) <= 0) {
                //if the worker thread has not waken up
                log.warn("The worker thread is not waked up:%s!", strerror(errno));
            }
        }
    } else {
        size_t new_task_index = task_index.fetch_add(1);
    }
}

void event_processor::notify_event() {
    status_code _status = this->status.load(std::memory_order_relaxed);
    log.debug("Try to wake up the main thread of event processor:%llu,status:%d!", id(), _status);
    if (thread_id() != std::this_thread::get_id() &&
        _status != working) {

        unsigned char temp[1] = {task_signal};
        if (write(pipe_fd[1], temp, 1) <= 0) {
            //if the worker thread has not waken up
            log.warn("The worker thread is not waked up:%s!", strerror(errno));
        }
    }
}

void event_processor::process_task_queue(int &index) {
    std::function<void()> task;
    while (!local_task_list.empty()) {
        if (local_task_list[0].second == index + 1) {
            local_task_list[0].first();
            local_task_list.pop_front();
            index++;

            continue;
        }

        if (task_list.try_pop(task)) {
            task();
            index++;
        } else {
            log.error("An queue fatal occurred in event_processor:%d!", id());
            throw std::runtime_error("Queue fatal in task queue of event processor!");
        }
    }

    while (task_list.try_pop(task)) {
        task();
        index++;
    }
}

io_factory event_processor::get_factory() {
    return io_factory(this);
}