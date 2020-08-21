//
// Created by suhang on 20-8-22.
//

#include "tcp_acceptor.h"

//readable but not writable, don't support epoll_rd_hup
const static io::io_type acceptor_type = {1, 0, 0};

tcp_acceptor::tcp_acceptor(const io::this_is_private &, const logger &_log) :
        io(this_is_private(0), socket(AF_INET, SOCK_STREAM, 0), io_type(), _log) {
    if (fd <= 0) {
        log.error("Fail to create a tcp acceptor,%s!", strerror(errno));
    }


}