//
// Created by suhang on 20-7-25.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <io/event_processor.h>
#include <io/io_factory.h>
#include "connection_pool.h"

unsigned short l_port = 9010;

class read_op : public io_op {
public:

    read_op(const logger &_log) : log(_log) {
        type = Read;
    }


    //for write operation, must return true
    virtual bool process(io_buffer &buffer) {
        while (buffer.size() != 0) {
            buffer.pop_front_to_other_back_n(l_buffer, buffer.size());
        }
        return true;
    }

    virtual void notify(result_type result) {
        if (result == Done) {

            return;
        }
    }

    //only used for write event;
    virtual size_t size() {
        return -1;
    }

    virtual const char *fail_message() {
        return "";
    };

private:
    logger log;
    io_buffer l_buffer;
};


void accept_test() {
    logger _log("accept_test", logger::DEBUG, true);
    event_processor processor(_log);
    io_factory factory(&processor);

    auto acceptor_future = factory.create_io_async<tcp_acceptor>(_log, l_port);
    assert(acceptor_future.get() != nullptr);
    pause();
}


void send_test() {
    logger _log("send_test", logger::DEBUG, true);
    event_processor processor(_log);
    io_factory factory(&processor);
    for (size_t i = 0; i < 1000; i++) {
        factory.create_tcp_async("127.0.0.1",l_port,_log);
    }
    pause();
}


int main() {
    pid_t fpid; //fpid represents the return value of fork function
    int count = 0;
    fpid = fork();
    if (fpid < 0) {
        printf("error in fork!");
    } else if (fpid == 0) {
        printf("I am the child process, my process id is %d\n", getpid());
        count++;
        send_test();
    } else {
        printf("I am the parent process, my process id is %d\n", getpid());
        count++;
        accept_test();
    }
    printf("count is: %d\n", count);
}