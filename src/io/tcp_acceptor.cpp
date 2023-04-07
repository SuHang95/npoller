//
// Created by suhang on 20-8-22.
//

#include "tcp_acceptor.h"
#include "event_processor.h"

//readable but not writable, don't support epoll_rd_hup
const static io::io_type acceptor_type = {1, 0};

tcp_acceptor::tcp_acceptor(const io::this_is_private &, const logger &_log) :
        io(this_is_private(0), socket(AF_INET, SOCK_STREAM, 0), io_type(), false, _log
        ) {
    if (fd <= 0) {
        log.error("Fail to create a tcp acceptor,%s!", strerror(errno));
        io::mark_invalid();
    }
}

void tcp_acceptor::start_accept(in_addr_t addr, unsigned short int port,
                                int queue_size, event_processor processor) {
    sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(addr);
    address.sin_port = htons(port);

    if (bind(fd,(struct sockaddr *) &address, sizeof(address)) < 0) {
        char s_addr[20];
        //we need _errno beacuse later there will be a inet_ntop call
        int _errno = errno;
        log.error("Bind address %d:%d fail,%s", inet_ntop(AF_INET, &addr, s_addr, 20),
                  port,
                  strerror(_errno)
        );

        mark_invalid();

        return;
    }

    if (listen(fd, queue_size) < 0) {
        char s_addr[20];
        //we need _errno beacuse later there will be a inet_ntop call
        int _errno = errno;
        log.error("Listen address %d:%d with queue size:%d fail,%s", inet_ntop(AF_INET, &addr, s_addr, 20),
                  port, queue_size,
                  strerror(_errno)
        );

        mark_invalid();

        return;
    }


}