//
// Created by suhang on 20-8-22.
//

#ifndef NPOLLER_TCP_ACCEPTOR_H
#define NPOLLER_TCP_ACCEPTOR_H

#include "tcp.h"


class tcp_acceptor : public io {
public:
    tcp_acceptor(const this_is_private &,  const logger &_log);

    void start_accept(in_addr_t addr,unsigned short int port,int queue_size,event_processor processor);

private:

};


#endif //NPOLLER_TCP_ACCEPTOR_H
