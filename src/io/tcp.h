#ifndef _NETPROCESSOR_TCP_H__
#define _NETPROCESSOR_TCP_H__


#include"io.h"

#include<sys/socket.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/un.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<future>

class tcp : public io {
public:
    enum tcp_status : unsigned char {
        initializing = 0,
        connecting = 1,
        connected = 2,
        // peer close or shutdown write or close() or we shut down read the socket
        peer_shutdown_write = 3,
        // we have shutdown write the connection
        shutdown_write = 4,
        closed = 5,
        invalid = 6
    };

    tcp(const this_is_private &, int, tcp_status, const logger &);

    tcp(const this_is_private &, const logger &_log);

    //register the event
    //virtual bool register(io_op *);

    virtual ~tcp() {
        if (connected_handler != nullptr) {
            delete connected_handler;
        }
    }

    void close() override;

    bool is_open() override;

    inline std::string descriptor() const;

    inline tcp_status status() const;

protected:


    void connect(const char *addr, unsigned short int port);

    void reconnect();

    inline void connect_complete();

    inline void handle_connect_result(bool is_connected);

    //only used by first constructor,which we don't know addr
    void set_addr_and_test();

    bool process_event(::epoll_event &) override;

    bool get_event(epoll_event &ev) override;

    void direct_read() override;

    void direct_write() override;

    bool valid_safe() override;

    bool readable_safe() override;

    bool writable_safe() override;

    void set_writable(bool) override;

    void set_readable(bool) override;

    bool valid_unsafe() override;

    bool readable_unsafe() override;

    bool writable_unsafe() override;

    void mark_invalid() override;


    //indicate the peer's address and port
    sockaddr_in address;

    unsigned short int port;
    //local address <--> peer address @ construct time
    std::string time_addr_str;

    std::atomic<tcp_status> __status;

    std::packaged_task<std::shared_ptr<tcp>(bool)> *connected_handler;

    //these inline function never use lock!
    inline sockaddr_in make_sockaddr_in(in_addr_t addr, int port);

    inline sockaddr_in make_sockaddr_in(const char *addr, int port);

    static inline io::io_type get_io_type(tcp_status _status);

    inline void set_connected_handler(std::packaged_task<std::shared_ptr<tcp>(bool)> *handler);

    inline tcp_status status_unsafe();

    inline void set_status(tcp_status _status);

    friend class event_processor;

    friend class io_factory;

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
            return io_type{0x01, 0x01};
        case peer_shutdown_write:
            return io_type{0, 0x01};
        case shutdown_write:
            return io_type{0x01, 0};
        default:
            return io_type{0, 0};
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

inline std::string tcp::descriptor() const {
    return std::to_string(fd) + ":" + time_addr_str;
}

inline tcp::tcp_status tcp::status() const {
    return __status.load(std::memory_order_relaxed);
}

inline void tcp::set_status(tcp::tcp_status _status) {
    __status.store(_status, std::memory_order_relaxed);
}

inline void tcp::set_connected_handler(std::packaged_task<std::shared_ptr<tcp>(bool)> *handler) {
    this->connected_handler = handler;
}

inline void tcp::handle_connect_result(bool is_connected) {
    if (connected_handler != nullptr) {
        (*connected_handler)(is_connected);
        delete connected_handler;
        connected_handler = nullptr;
    }
}

inline void tcp::connect_complete() {
    set_status(connected);
    set_write_busy(false);
    support_epollrdhup = true;
    handle_connect_result(true);
}

#endif
