//
// Created by suhang on 20-8-22.
//

#ifndef NPOLLER_TCP_ACCEPTOR_H
#define NPOLLER_TCP_ACCEPTOR_H

#include "tcp.h"

const int default_queue_size = 1000;

class tcp_acceptor :  public io {
public:
    tcp_acceptor(const this_is_private &, const logger &_log,
                 const std::function<void(std::shared_ptr<tcp>&&,const std::string&)>&,
                 unsigned short int port, in_addr_t addr = INADDR_ANY);

    bool start_accept(event_processor *processor, int queue_size = default_queue_size);

    void direct_read() override;

private:
    sockaddr_in address;
    const std::function<void(std::shared_ptr<tcp>&&,const std::string&)> callback;
};


#endif //NPOLLER_TCP_ACCEPTOR_H
