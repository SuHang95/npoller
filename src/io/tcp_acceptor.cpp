//
// Created by suhang on 20-8-22.
//

#include "tcp_acceptor.h"
#include "event_processor.h"

//readable but not writable, don't support epoll_rd_hup
const static io::io_type acceptor_type = {1, 0};

tcp_acceptor::tcp_acceptor(const io::this_is_private &, const logger &_log, unsigned short int port, in_addr_t addr) :
        io(this_is_private(0), socket(AF_INET, SOCK_STREAM, 0), acceptor_type, false, _log
        ) {
    if (fd <= 0) {
        log.error("Fail to create a tcp acceptor,%s!", strerror(errno));
        io::mark_invalid();
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(addr);
    address.sin_port = htons(port);
}

bool tcp_acceptor::start_accept(event_processor *processor, int queue_size) {
    if (bind(fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        char s_addr[INET_ADDRSTRLEN];
        //we need _errno because later there will be an inet_ntop call
        int _errno = errno;
        log.error("Bind address %d:%d fail,%s", inet_ntop(AF_INET, &(address.sin_addr), s_addr, INET_ADDRSTRLEN),
                  ntohs(address.sin_port),
                  strerror(_errno)
        );

        mark_invalid();

        return false;
    }

    if (listen(fd, queue_size) < 0) {
        char s_addr[INET_ADDRSTRLEN];
        //we need _errno because later there will be an inet_ntop call
        int _errno = errno;
        log.error("Listen address %d:%d with queue size:%d fail,%s",
                  inet_ntop(AF_INET, &(address.sin_addr), s_addr, INET_ADDRSTRLEN),
                  ntohs(address.sin_port), queue_size,
                  strerror(_errno)
        );

        mark_invalid();

        return false;
    }

    log.debug("tcp acceptor:%d has listen successfully!", fd);
    std::shared_ptr<tcp_acceptor> this_ptr = shared_from_this();

    log.debug("Try to add the acceptor instance %d to event processor!", fd);
    if (!processor->add_io(std::dynamic_pointer_cast<io>(this_ptr))) {
        return false;
    }

    return true;
}

void tcp_acceptor::direct_read() {
    // Accept connections
    sockaddr_in peer_address;
    socklen_t peer_address_len = sizeof(peer_address);
    int retry_times = 0;
    bool need_retry;
    do {
        need_retry = false;
        int client_fd = accept(fd, (sockaddr *) &peer_address, &peer_address_len);
        if (client_fd < 0) {
            switch (errno) {
                case EBADF:
                    mark_invalid();
                    log.error("Accept on fd %d fail,%s", fd, strerror(errno));
                case EINVAL:
                    close();
                    log.error("Accept on fd %d fail,%s!local address:%s, port:%d.", fd,
                              strerror(errno), inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    return;
                case EINTR:
                    need_retry = true;
                    break;
                case EAGAIN:
                    return;
                default:
                    log.error("Accept on fd %d fail,%s!local address:%s, port:%d.", fd,
                              strerror(errno), inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    return;
            }
            continue;
        }
        std::shared_ptr<tcp> client_tcp = std::make_shared<tcp>(this_is_private(0), client_fd,
                                                                tcp::tcp_status::connected,
                                                                this->log);
        this->manager.load(std::memory_order_relaxed)->add_io(client_tcp);
        log.debug("Accept a tcp connection:%s", client_tcp->descriptor().c_str());
    } while (need_retry && retry_times++ < 3);
}