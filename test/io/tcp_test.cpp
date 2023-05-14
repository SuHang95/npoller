//
// Created by suhang on 20-7-25.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <io/event_processor.h>
#include <io/io_factory.h>
#include "connection_pool.h"

unsigned short l_port = 9010;
int nums = 10000;

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
    logger _log("accept_test", logger::WARN, true);
    event_processor processor(_log);
    io_factory factory(&processor);

    auto acceptor_future = factory.create_io_async<tcp_acceptor>(_log, l_port);
    assert(acceptor_future.get() != nullptr);
    sleep(5);
}


void send_test() {
    logger _log("send_test", logger::DEBUG, false);
    event_processor processor(_log);
    io_factory factory(&processor);

    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;

    std::chrono::microseconds _start = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());

    for (size_t i = 0; i < nums; i++) {
        std::atomic_int counter;
        factory.create_io_with_callback<tcp>(
                std::packaged_task([&](std::shared_ptr<tcp> &&tcp, const std::string &message) {
                    if (counter.fetch_add(1) == nums - 1) {
                        {
                            std::unique_lock<std::mutex> lk(mtx);
                            ready = true;
                        }
                        cv.notify_one();
                    }
                    if (!message.empty()) {
                        _log.debug("Create tcp fail!" + message);
                    }
                }),
                "127.0.0.1", l_port, _log);
    }

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&] { return ready; });
    std::chrono::microseconds _end = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    std::cout << "Has created " << nums << " connections!" << std::endl;
    std::cout << "Time cost for send:" << _end.count() - _start.count() << std::endl;
    return;
}


int main() {
    std::thread t([](){
        accept_test();
    });
    send_test();
    t.join();
}