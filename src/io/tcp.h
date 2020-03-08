#ifndef _NETPROCESSOR_TCP_H__
#define _NETPROCESSOR_TCP_H__


#include"io.h"

#include<sys/socket.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/un.h>
#include<netinet/in.h>
#include<arpa/inet.h>


class tcp : public io {
public:
    enum tcp_status : unsigned char {
        initializing = 0,
        connecting = 1,
        connected = 2,
        // peer close or shutdown write or close() or we shutdown read the socket
        peer_shutdown_write = 3,
        // we have shutdown write the connection
        shutdown_write = 4,
        closed = 5
    };

    tcp(const this_is_private &, int, tcp_status, const logger &);

    tcp(const this_is_private &, const logger &_log, const char *addr, unsigned short int port);

    tcp(const this_is_private &, const logger &_log, in_addr_t addr = INADDR_ANY, unsigned short int port = 0);

    //regist the event
    //virtual bool regist(io_op *);

    void close();

    inline std::string descriptor() const;

    inline tcp_status status() const;

protected:


    void connect(const char *addr, unsigned short int port);

    //only used by first constructor,which we don't know addr
    void set_addr_and_test();

    virtual bool process_event(::epoll_event &);

    virtual bool get_event(epoll_event &ev);

    virtual void process_read();

    virtual void direct_read();

    virtual void direct_write();

    void reconnect();

    //indicate the peer's address and port
    sockaddr_in address;

    unsigned short int port;
    //local address <--> peer address @ construct time
    std::string time_addr_str;

    std::atomic<tcp_status> __status;

    //these inline function never use lock!
    inline sockaddr_in make_sockaddr_in(in_addr_t addr, int port);

    inline sockaddr_in make_sockaddr_in(const char *addr, int port);

    inline void __close();

    inline void prepare_close();

    static inline io::io_type get_io_type(tcp_status _status);



    inline tcp_status status_unsafe();

    inline void set_status(tcp_status _status);

};

inline sockaddr_in tcp::make_sockaddr_in(const char *__addr, int __port) {
    sockaddr_in address;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(__addr);
    address.sin_port = htons(__port);

    return address;
}

inline io::io_type tcp::get_io_type(tcp_status _status) {
    io_type _type;

    switch (_status) {
        case initializing:
        case connecting:
        case connected:
            _type.readable = 1;
            _type.writable = 1;
            _type.support_epollrdhup = 1;
            return _type;
        case peer_shutdown_write:
            _type.readable = 0;
            _type.writable = 1;
            _type.support_epollrdhup = 0;
            return _type;
        case shutdown_write:
            _type.readable = 1;
            _type.writable = 0;
            _type.support_epollrdhup = 1;
            return _type;
    }
}

inline sockaddr_in tcp::make_sockaddr_in(in_addr_t __addr, int __port) {
    sockaddr_in address;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(__addr);;
    address.sin_port = htons(__port);

    return address;
}

inline tcp::tcp_status tcp::status_unsafe() {
    return *(reinterpret_cast<tcp_status *>(&__status));
}


inline tcp::tcp_status tcp::status() const{
    return __status.load(std::memory_order_relaxed);
}

inline void tcp::set_status(tcp::tcp_status _status) {
    __status.store(_status, std::memory_order_relaxed);
}

inline void tcp::__close() {
    ::close(fd);
    set_status(closed);
    set_valid(false);
}

inline void tcp::prepare_close() {
    if (status() != closed) {
        port = 0;
        set_writable(false);
        set_readable(false);
    }
}

#endif
