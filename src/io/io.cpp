//
// Created by suhang on 2020/1/26.
//

#include "io.h"
#include "event_processor.h"


io::io(const this_is_private &p, const int _fd, io_type type,
       bool _support_epollrdhup, bool nonblock, const logger &_log) :
        fd(_fd), log(_log), valid(true), manager(nullptr),
        support_epollrdhup(_support_epollrdhup) {
    if (fd < 0) {
        mark_invalid();
    }

    if (!nonblock) {
        int ret = fcntl(fd, F_SETFL, O_NONBLOCK);

        if (ret < 0) {
            mark_invalid();
            log.error("Set fd %d non-blocking error,%s", fd, strerror(errno));
        }
    }

    have_write = 0;
    set_write_busy(false);
    set_readable(type.readable == 1);
    set_writable(type.writable == 1);
}


io::~io() {
    if (valid_safe()) {
        ::close(fd);
    }
}


void io::update_manager(event_processor *__manager) {
    if (get_manager_unsafe() != nullptr) {
        get_manager_unsafe()->remove_io(fd);
    }
    manager.store(__manager, std::memory_order_relaxed);
}

void io::mark_invalid() {
    update_manager(nullptr);
    valid.store(false, std::memory_order_relaxed);
    set_readable(false);
    set_writable(false);
}

bool io::process_event(::epoll_event &ev) {
    if (ev.events & EPOLLIN) {
        this->direct_read();
        this->process_read();
    }

    if (ev.events & EPOLLRDHUP) {
        set_readable(false);
        this->clean_read();
    }

    this->write_flush();

    if ((ev.events & EPOLLOUT) || !write_busy_unsafe()) {
        this->direct_write();
        this->process_write();
    }

    if (!readable_unsafe()) {
        clean_read();
    }

    if (!writable_unsafe()) {
        this->clean_write();
    }

    return get_event(ev);
}

bool io::get_event(epoll_event &ev) {
    memset(&ev, 0, sizeof(ev));

    if (!readable_unsafe() && !writable_unsafe()) {
        return false;
    }
    if (readable_unsafe()) {
        ev.events |= EPOLLIN;
    }
    if (support_epollrdhup && readable_unsafe()) {
        ev.events |= EPOLLRDHUP;
    }

    //if the io can use write and it cant read and is not busy,we will use EPOLLET
    if (writable_unsafe()) {
        if (readable_unsafe()) {
            if (write_busy_unsafe())
                ev.events |= EPOLLOUT;
        } else {
            ev.events |= (EPOLLOUT | EPOLLET);
        }
    }

    ev.data.fd = fd;
    return true;
}


//read start with the end
void io::direct_read() {
    if (!valid_unsafe() || !readable_unsafe()) {
        log.debug("Attempt reading an invalid or unreadable io is !");
        return;
    }
#ifdef _debug
    int sum=0;
#endif

    char *read_buffer_last_block_ptr = nullptr;
    size_t can_read_bytes = 0;
    int retry_times = 0;
    while (true) {
        if ((read_buffer.end()) < (read_buffer._buffer().size() * DEFAULT_SIZE)) {
            can_read_bytes = DEFAULT_SIZE - (read_buffer.end() % DEFAULT_SIZE);
            read_buffer_last_block_ptr = read_buffer._buffer()[read_buffer.end() / DEFAULT_SIZE];
        } else {
            read_buffer_last_block_ptr = io_buffer::mem.get();
            read_buffer._buffer().push_back(read_buffer_last_block_ptr);
            can_read_bytes = DEFAULT_SIZE;
        }
        int ret = read(fd, read_buffer_last_block_ptr +
                           read_buffer.end() % DEFAULT_SIZE, can_read_bytes);
        if (ret < 0) {

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                retry_times++;
                if (retry_times == max_retry_times) {
                    break;
                }
                continue;
            } else if (errno == EINVAL) {
#ifdef _debug
                if(read_buffer_lastblock_ptr==nullptr)
                    throw std::logic_error("Wrong pointor!");
#endif
                log.error("buf %lu is outside your accessible address space! fd:%d.",
                          reinterpret_cast<std::size_t>(read_buffer_last_block_ptr), fd);
                break;
            } else if (errno == EBADF) {
                //!!!Be Attention!!!
                mark_invalid();
                log.error("Illegal read attempt to an invalid fd:%d!", fd);
                break;
            } else {
                log.error("Read %zd bytes from file descriptor %d fail,%s!", can_read_bytes, fd, strerror(errno));
                break;
            }
        } else if (ret == 0) {
            set_readable(false);
            support_epollrdhup = false;
            log.debug("Read the io %d,but return with 0,maybe EOF!", fd);
            break;
        } else {

#ifdef _debug
            sum+=ret;
#endif

            read_buffer.end() += ret;
            if (read_buffer.begin() == size_t(-1))
                read_buffer.begin() = 0;
        }
    }

    if ((read_buffer.begin() == (size_t) -1) && (read_buffer.end() > 0))
        read_buffer.end() = 0;
#ifdef _debug
    if(log_used){
        log.debug("direct_read reutrn with have read %d bytes",sum);
    }
#endif

}

//write start with the begin
void io::direct_write() {
    if (!valid_unsafe() || !writable_unsafe()) {
        return;
    }

    char *write_buffer_first_block_ptr = nullptr;
    size_t can_write_bytes = 0;
    int retry_times = 0;
    unsigned int write_sum = 0;

    while (true) {
        if ((write_buffer.begin()) != -1 && (write_buffer.end()) && ((write_buffer.begin()) != (write_buffer.end()))) {
            //if begin and end is in one block,the bytes can be write is size,other is the left size in first block
            can_write_bytes = ((write_buffer.end() / DEFAULT_SIZE) == ((write_buffer.begin() / DEFAULT_SIZE)) ? \
                write_buffer.end() - write_buffer.begin() : DEFAULT_SIZE - (write_buffer.begin()) % DEFAULT_SIZE);
            write_buffer_first_block_ptr = write_buffer._buffer()[write_buffer.begin() / DEFAULT_SIZE];
        } else {
            break;
        }
        int ret = write(fd, write_buffer_first_block_ptr, can_write_bytes);
        if (ret <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                set_write_busy(true);
                break;
            } else if (errno == EINTR) {
                retry_times++;
                if (retry_times == max_retry_times) {
                    break;
                }
                continue;
            } else if (errno == EINVAL) {
                if (write_buffer_first_block_ptr == nullptr)
                    throw std::logic_error("Wrong pointer!");
                set_writable(false);
                break;
            } else if (errno == EBADF) {
                //!!!Be attention!!!
                mark_invalid();
                break;
            } else if (errno == EPIPE) {
                set_writable(false);
                break;
            } else if (ret == 0) {
                break;
            } else {
                log.error("Write %zd bytes from file descriptor %d fail!", write_sum, fd);
                break;
            }
        } else {
            write_buffer.begin() += ret;
            write_sum += ret;
        }
    }
    if ((write_buffer.begin()) == write_buffer.end()) {
        write_buffer.begin() = -1;
        write_buffer.end() = 0;
    }

    //not use shrink_size() is because the shrink_size() use the lock,and __shrink_size is a inline function which do not use lock
    write_buffer.__shrink_size();

    have_write += write_sum;
}


bool io::do_register(const std::shared_ptr<io_op> &__event) {
    if (!valid_safe() || get_manager_safe() == nullptr) {
        return false;
    }

    if (__event->io_id != io_op::unspecified_id &&
        __event->io_id != fd) {
        return false;
    }

    if (__event->type == io_op::Read) {
        if (!readable_safe()) {
            return false;
        }
        read_list.push(__event);
    } else if (__event->type == io_op::Write) {
        if (!writable_safe()) {
            return false;
        }
        write_list.push(__event);

        event_processor *manager = this->get_manager_safe();
        if (!write_busy_safe() && !(manager == nullptr)) {
            manager->add_write_instant_task(fd);
        }
    }
    return true;
}


void io::process_read() {
    if (temp_read_event.get() != nullptr) {
        if (temp_read_event->process(this->read_buffer)) {
            temp_read_event->notify(io_op::Done);
            temp_read_event = nullptr;
        } else {
            return;
        }
    }

    while (!read_list.empty()) {
        std::shared_ptr<io_op> event;
        if (!read_list.try_pop(event))
            continue;

        if (event->process(this->read_buffer)) {
            event->notify(io_op::Done);
        } else {
            temp_read_event = event;
            return;
        }
    }
}

void io::clean_read() {
    process_read();
}


void io::write_flush() {
    while (!write_list.empty()) {
        std::shared_ptr<io_op> event;
        if (!write_list.try_pop(event)) {
            continue;
        }

        size_t before_size = this->write_buffer.size();
        if (!event->process(this->write_buffer)) {
            log.error("Some io event write fail,%s!", event->fail_message());
        }

        wait_write_list.push_back(event);
    }
}

void io::process_write() {
    while (!wait_write_list.empty()) {
        auto it = wait_write_list.begin();

        if (have_write >= ((*it)->size())) {
            have_write -= (*it)->size();

            (*it)->notify(io_op::Done);

            if (!wait_write_list.empty())
                wait_write_list.erase(it);
        } else {
            break;
        }
    }
}

void io::clean_write() {
    while (!write_list.empty()) {
        std::shared_ptr<io_op> event;
        if (!write_list.try_pop(event)) {
            continue;
        }
        event->notify(io_op::IOError);
    }
}


