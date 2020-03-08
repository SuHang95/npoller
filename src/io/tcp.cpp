#include"tcp.h"
#include"event_processor.h"

tcp::tcp(const this_is_private &p, int _fd, tcp_status _status, const logger &_log) :
        io(this_is_private(0), _fd, get_io_type(_status), _log), __status(_status) {
    if (_status == connected || _status == peer_shutdown_write || _status == shutdown_write) {
        set_addr_and_test();
    }
}

tcp::tcp(const this_is_private &p, const logger &_log, const char *addr, unsigned short int port) :
        tcp(this_is_private(0), socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0), initializing, _log) {
    if (fd < 0) {
        set_valid(false);
        log.error("Create a socket instance fail!");
        return;
    }

    address = make_sockaddr_in(addr, port);
    auto address_len = sizeof(address);

    int ret = bind(fd, (struct sockaddr *) &address, address_len);
    if (ret < 0) {
        log.error("bind fail:%s!", strerror(errno));
    }
}

tcp::tcp(const this_is_private &p, const logger &_log, in_addr_t addr, unsigned short int port) :
        tcp(this_is_private(0), socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0),
            initializing, _log) {
    if (fd < 0) {
        set_valid(false);
        log.error("Create a socket instance fail!");
        return;
    }

    address = make_sockaddr_in(addr, port);
    auto address_len = sizeof(address);

    int ret = bind(fd, (struct sockaddr *) &address, address_len);
    if (ret < 0) {
        log.error("bind fail!");
    }
}


void tcp::connect(const char *__addr, unsigned short int __port) {
    sockaddr_in other = make_sockaddr_in(__addr, __port);
    socklen_t other_len = sizeof(sockaddr_in);
    int ret;

    int retry_times = 0;

    //only if tcp is not on,we can use the connect function
    if (status() != initializing || !valid_unsafe()) {
        return;
    }
    ret = ::connect(fd, (sockaddr *) &other, other_len);
    if (ret < 0) {
        if (errno == EINPROGRESS) {
            set_status(connecting);

            set_readable(false);
            set_writable(false);

            address = other;
            port = __port;

        } else if (errno == ECONNREFUSED) {
            log.error("The socket %d try to connect %s:%d,"\
                    "No one listening in the remote address!", fd, __addr, __port);
            __close();
        } else if (errno == EALREADY) {
            set_status(connecting);
        }  else if (errno == EISCONN) {
            set_status(connected);
            set_readable(true);
            set_writable(true);
            set_write_busy(true);
            support_epollrdhup = true;
        } else if (errno == ENETUNREACH) {
            log.error("Network is unreachable!");
            tcp::__close();
        } else {
            log.error("Try connect the %s:%d,some error occur %s!", \
                    __addr, port, strerror(errno));
            tcp::__close();
        }
    } else {
        set_status(connected);
        set_readable(true);
        set_writable(true);
        set_write_busy(false);
        support_epollrdhup=true;

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
            set_valid(false);
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
    this->tcp::prepare_close();

    clean_read();
    clean_write();

    set_manager(nullptr);

    if (valid_unsafe())
        this->tcp::__close();
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

    if (write_busy_unsafe() == 0) {
        direct_write();
        process_write();
    }

    if (readable_unsafe() == 0 && writable_unsafe() == 0) {
        if (get_manager_unsafe() != nullptr) {
            get_manager_unsafe()->remove_io(fd);
            set_manager(nullptr);
        }
        this->tcp::__close();
        clean_read();
        clean_write();
        log.debug("We have clean the event list from %d and close it!", fd);
        return false;
    }


    if (status_unsafe() == peer_shutdown_write || readable_unsafe() == 0) {
        if (get_manager_unsafe() != nullptr) {
            get_manager_unsafe()->remove_io(fd);
            set_manager(nullptr);
        }
        this->tcp::__close();
        clean_read();
        clean_write();
        log.debug("We have clean the event list from %d and close it!", fd);
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
    tcp_status l_status = status_unsafe();
    if (l_status == initializing || l_status == connecting) {
        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.fd = fd;
        return true;
    } else if (l_status == connected) {
        ev.events = EPOLLIN | EPOLLRDHUP;
        if (write_busy_unsafe())
            ev.events |= EPOLLOUT;
        ev.data.fd = fd;
        return true;
    } else if (l_status == peer_shutdown_write) {
        ev.events = EPOLLOUT;
        ev.data.fd = fd;

        set_readable(false);
        support_epollrdhup = false;

        return true;
    } else if (l_status == shutdown_write) {
        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.fd = fd;

        set_writable(false);
        support_epollrdhup = false;

        return true;
    } else if (l_status == closed) {
        if (get_manager_unsafe() != nullptr) {
            get_manager_unsafe()->remove_io(fd);
            set_manager(nullptr);
        }
        this->tcp::__close();
        this->process_read();
        this->process_write();

        return false;
    } else {
        if (get_manager_unsafe() != nullptr) {
            get_manager_unsafe()->remove_io(fd);
            set_manager(nullptr);
        }
        this->tcp::__close();
        return false;
    }

    return true;
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
            this->tcp::prepare_close();
            return;
        }
        //In general,error can't be EINPROGRESS
        if (err == EINPROGRESS || err == EALREADY) {
            set_status(connecting);
            return;
        } else if (err == EISCONN) {
            set_status(connected);
            set_readable(true);
            set_writable(true);
            return;
        } else if (err == ENETUNREACH) {
            log.error("Network is unreachable!");
            this->tcp::prepare_close();
            return;
        } else {
            log.error("The socket pair %s connected fail %s",
                      time_addr_str.c_str(), strerror(errno));
            this->tcp::prepare_close();
            return;
        }
    } else {
        set_status(connected);
        set_readable(true);;
        return;
    }
}


void tcp::direct_read() {
    io::direct_read();
    if (!valid_unsafe()) {
        return;
    }

    //don't know what does the code below mean
    /*if (readable_unsafe()) {
        if (writable_unsafe() || connect_status == 2) {
            connect_status = 3;
            log.debug("A TCP connection %d has been close or shutdown write by peer,"\
                "Its tcp_status turns to 3", this->fd);
        } else {
            log.info("A tcp connection %d can't read with tcp_status=%d"\
                "can write=%d", fd, static_cast<int>(connect_status.load()),
                     reinterpret_cast<uint8_t>(writable_unsafe.load()));
            if (is_managed && get_manager_unsafe != nullptr) {
                get_manager_unsafe->remove_io(fd);
                get_manager_unsafe == nullptr;
                is_managed = 0;
            }
            this->tcp::__close();
        }
    }*/
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
/*
bool tcp::regist(io_op *__event) {
    if (__event->io_id != fd || !valid_safe())
        return false;
    if (__event->type == ReadEvent) {
        if (!readable_safe())
            return false;

        read_list.push(__event);
        log.debug("Regist a read event on io %d", fd);

    } else if (__event->type == WriteEvent) {
        if (writable_unsafe == 0 && connect_status >= 4)
            return false;

        write_list.push(__event);

        if (write_busy_unsafe == 0 && is_managed != 0 && get_manager_unsafe != nullptr && connect_status >= 2) {
            get_manager_unsafe->add_write_instant_task(fd);
        }


        log.debug("Regist a write event on io %d", fd);
    }
    return true;
}*/



