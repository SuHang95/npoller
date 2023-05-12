#include"tcp.h"
#include"event_processor.h"

tcp::tcp(const this_is_private &p, int _fd, tcp_status _status, const logger &_log) :
        io(this_is_private(0), _fd, get_io_type(_status), true, _log), __status(_status) {
    if (_status == connected || _status == peer_shutdown_write || _status == shutdown_write) {
        set_addr_and_test();
    }
}

tcp::tcp(const this_is_private &p, const logger &_log) :
        tcp(this_is_private(0), socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0), initializing, _log) {
    if (fd < 0) {
        mark_invalid();
        log.error("Create a socket instance fail!");
        return;
    }
}

void tcp::connect(const char *__addr, unsigned short int __port) {
#ifdef _debug
    assert(std::this_thread::get_id() == this->get_manager_safe()->thread_id());
#endif
    sockaddr_in other = make_sockaddr_in(__addr, __port);
    socklen_t other_len = sizeof(sockaddr_in);
    int ret;

    //only if tcp is not on,we can use the connect function
    if (status() != initializing || !valid_unsafe()) {
        return;
    }
    ret = ::connect(fd, (sockaddr *) &other, other_len);
    if (ret < 0) {
        //actually, most situation it will return EINPROGRESS
        if (errno == EINPROGRESS) {
            set_status(connecting);
            address = other;
            port = __port;

        } else if (errno == EALREADY) {
            set_status(connecting);
        } else if (errno == EISCONN) {
            connect_complete();
        } else if (errno == ENETUNREACH) {
            log.error("Network is unreachable!");
            tcp::internal_close();
        } else {
            log.error("Try connect the %s:%d,some error occur %s!", \
                    __addr, port, strerror(errno));
            tcp::internal_close();
        }
    } else {
        connect_complete();
        address = other;
        port = __port;
    }
}

void tcp::set_addr_and_test() {
    struct sockaddr_in peer;
    struct sockaddr_in local;

    socklen_t size = sizeof(sockaddr_in);
    if (::getpeername(fd, (sockaddr *) &peer, &size) < 0) {
        if (errno == EBADF || errno == ENOTSOCK || errno == ENOTCONN) {
            //for ENOTCONN,we don't know peer address,also make this fd invalid
            mark_invalid();
            return;
        } else {
            log.error("Query tcp peer name fail:%s", strerror(errno));
            return;
        }
    }
    std::string peer_name = inet_ntoa(peer.sin_addr) + ntohs(peer.sin_port);

    if (::getpeername(fd, (sockaddr *) &local, &size) < 0) {
        log.error("Query tcp local addr fail:%s", strerror(errno));
        return;
    }
    std::string local_name = inet_ntoa(local.sin_addr) + ntohs(local.sin_port);

    std::string time = log.time(log_time_strategy::millisecond);

    time_addr_str = local_name + "<-->" + peer_name + "@" + time;
}

void tcp::close() {
    clean_read();
    clean_write();

    if (get_manager_unsafe() != nullptr) {
        get_manager_unsafe()->remove_io(fd);
        //set_manager(nullptr);
    }

    if (valid_unsafe()) {
        this->tcp::internal_close();
    }
}

bool tcp::process_event(::epoll_event &ev) {
    if (!valid_unsafe()) {
        return false;
    }
    switch (status()) {
        case initializing:
            break;
        case connecting:
            reconnect();
            if (status() == connecting) {
                return get_event(ev);
            } else if (status() == connected) {
                break;
            } else {
                close();
                handle_connect_result(false);
                return false;
            }
        case connected:
            break;
        case peer_shutdown_write:
            if (ev.events & EPOLLIN) {
                log.warn("For a tcp peer close or shutdown write"\
                    ",occurred an EPOLLIN event,we will close it!");
                this->process_write();
                close();
                return false;
            }
            break;
        case shutdown_write:
            if (ev.events & EPOLLOUT) {
                log.warn("For a tcp shutdown write,occurred an"\
                        "EPOLLOUT event,we will close it!");
                set_writable(false);
            }
            break;
        case closed:
            close();
            return false;
    }

    if (ev.events & EPOLLIN) {
        log.debug("An EPOLLIN event occurred on file descriptor %d", fd);
        direct_read();
        process_read();
    }

    //check if can_be_read
    if (!readable_unsafe()) {
        clean_read();
    }

    //check if peer shutdown or close
    if (ev.events & EPOLLRDHUP) {
        log.debug("An EPOLLRDHUP event occurred on file descriptor %d", fd);
        set_readable(false);
        clean_read();
    }

    write_flush();

    if (ev.events & EPOLLOUT) {
        log.debug("An EPOLLOUT event occurred on file descriptor %d", fd);

    }

    if (!write_busy_unsafe()) {
        direct_write();
        process_write();
    }

    if (status_unsafe() == closed) {
        close();
        return false;
    }


    if (status_unsafe() == peer_shutdown_write) {
        //TODO to make sure the logic when we get peer shut down
        //temporarily we will close the connection
        close();
        return false;
    }

    return get_event(ev);
}

bool tcp::get_event(epoll_event &ev) {
    memset(&ev, 0, sizeof(ev));
    if (valid_unsafe() == 0) {
        return false;
    }

    ev.events |= EPOLLET;
    tcp_status current_status = status_unsafe();
    switch (current_status) {
        case initializing:
        case connecting:
            ev.events = EPOLLIN | EPOLLOUT;
            ev.data.fd = fd;
            return true;
        case connected:
            ev.events = EPOLLIN | EPOLLRDHUP;
            if (write_busy_unsafe())
                ev.events |= EPOLLOUT;
            ev.data.fd = fd;
            return true;
        case peer_shutdown_write:
            ev.events = EPOLLOUT;
            ev.data.fd = fd;
            support_epollrdhup = false;
            return true;
        case shutdown_write:
            ev.events = EPOLLIN | EPOLLRDHUP;
            ev.data.fd = fd;
            return true;
        case closed:
            close();
            return false;
    }
}


void tcp::reconnect() {
    if (valid_unsafe() == 0 || status_unsafe() != connecting) {
        return;
    }

    if (::connect(fd, (struct sockaddr *) &(this->address), sizeof(sockaddr_in)) != 0) {
        int err = 0;
        socklen_t len;
        len = sizeof(err);
        //use getsockopt() to get and clear the error associated with fd
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
            log.error("Get socket option failed %s!", strerror(errno));
            this->set_status(closed);
            return;
        }
        //In general,error can't be EINPROGRESS
        if (err == EINPROGRESS || err == EALREADY) {
            set_status(connecting);
            return;
        } else if (err == EISCONN) {
            connect_complete();
            return;
        } else if (err == ENETUNREACH) {
            log.error("Network is unreachable!");
            this->set_status(closed);
            return;
        } else {
            log.error("The socket pair %s connected fail %s",
                      time_addr_str.c_str(), strerror(errno));
            this->set_status(closed);
            return;
        }
    } else {
        connect_complete();
        return;
    }
}


void tcp::direct_read() {
    io::direct_read();
    if (!valid_unsafe()) {
        return;
    }

    //in some scenarios, direct_read will mark the io as unreadable, so we need check the status of tcp
    //and do corresponding actions
    auto current_status = status_unsafe();

    if (current_status == peer_shutdown_write) {
        log.debug("A TCP connection %d has been closed or shutdown write by peer!", fd);
    } else if (current_status == closed) {
        log.info("A tcp connection %d will close", fd);
        close();
    }
}

void tcp::direct_write() {
    io::direct_write();
    if (!valid_unsafe()) {
        return;
    }
    if (!writable_unsafe()) {
        if (readable_unsafe() && status() == connected) {
            set_status(shutdown_write);
        } else {
            log.info("A tcp connection %d:%s can't read!",
                     fd, time_addr_str.c_str());
            this->close();
        }
    }
}

bool tcp::valid_safe() {
    return status() != invalid;
}

bool tcp::readable_safe() {
    auto current_status = status();
    return current_status == connected || current_status == shutdown_write;
}

bool tcp::writable_safe() {
    auto current_status = status();
    return current_status == connected || current_status == peer_shutdown_write;
}

bool tcp::valid_unsafe() {
    auto current_status = status_unsafe();
    return current_status != invalid;
}

bool tcp::readable_unsafe() {
    auto current_status = status_unsafe();
    return current_status == connected || current_status == shutdown_write;
}

bool tcp::writable_unsafe() {
    auto current_status = status_unsafe();
    return current_status == connected || current_status == peer_shutdown_write;
}

void tcp::mark_invalid() {
    __status.store(invalid, std::memory_order_relaxed);
}

void tcp::set_readable(bool _readable) {
    tcp_status current_status = status();
    tcp_status next_status = current_status;
    bool success;
    do {
        switch (current_status) {
            case initializing:
            case connecting:
                if (_readable) {
                    next_status = connected;
                }
                break;
            case connected:
                if (!_readable) {
                    next_status = peer_shutdown_write;
                    //we add this because currently we don't use the shutdown api with read, the behaviour is strange!
                    log.warn("Try to mark a connected tcp instance %s to unreadable!", descriptor().c_str());
                }
                break;
            case peer_shutdown_write:
                if (_readable) {
                    next_status = connected;
                    log.warn("Try to mark an unreadable tcp instance %s to connected!", descriptor().c_str());
                }
                break;
            case shutdown_write:
                if (!_readable) {
                    next_status = closed;
                }
        }
        success = __status.compare_exchange_strong(current_status, next_status, std::memory_order_relaxed,
                                                   std::memory_order_relaxed);
    } while (!success);
}


void tcp::set_writable(bool _writable) {
    tcp_status current_status = status();
    tcp_status next_status = current_status;
    bool success;
    do {
        switch (current_status) {
            case initializing:
            case connecting:
                if (_writable) {
                    next_status = connected;
                }
                break;
            case connected:
                if (!_writable) {
                    next_status = shutdown_write;
                }
                break;
            case peer_shutdown_write:
                if (!_writable) {
                    next_status = closed;
                }
                break;
            case shutdown_write:
                if (_writable) {
                    next_status = connected;
                    log.warn("Try to mark an un-writable tcp instance %s to writable!", descriptor().c_str());
                }
        }
        success = __status.compare_exchange_strong(current_status, next_status, std::memory_order_relaxed,
                                                   std::memory_order_relaxed);
    } while (!success);
}



