//
// Created by suhang on 20-7-25.
//

#include <bits/socket.h>
#include <netinet/in.h>
#include <io/event_processor.h>
#include <io/io_factory.h>
#include "connection_pool.h"

unsigned short l_port = 9010;

class read_op : public io_op {
public:

    read_op(const logger& _log) : log(_log) {
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


int main() {
    pid_t fpid; //fpid表示fork函数返回的值
    int count = 0;
    fpid = fork();
    if (fpid < 0) {
        printf("error in fork!");
    } else if (fpid == 0) {
        printf("i am the child process, my process id is %d\n", getpid());
        count++;
    } else {
        printf("i am the parent process, my process id is %d\n", getpid());
        count++;
    }
    printf("count is: %d\n", count);
}

void accept_test() {
    logger log("acceptLog", logger::DEBUG, true);
    event_processor processor1(log),processor2(log);
    connection_pool pool1,pool2;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    socklen_t len;


    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(l_port);

    int ret = bind(listen_fd, (struct sockaddr *) &address, sizeof(address));

    assert(ret >= 0);
    sockaddr_in in_addr;

    ret = listen(listen_fd, 10000);
    assert(ret >= 0);

    while (true) {
        io::io_type type;
        type.support_epollrdhup = 0;
        type.writable = 1;
        type.readable = 0;

        std::function<void(std::shared_ptr<tcp>)> callback = [=](std::shared_ptr<tcp> io_ptr) {
            (*io_ptr).regist()
        };

        int fd = ::accept(listen_fd, (sockaddr *) &in_addr, &len);
        processor1.get_factory().create_io_with_callback<tcp>(callback, 1, type, processor1.get_logger());
    }
}


void send_test() {

}