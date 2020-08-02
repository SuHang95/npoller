//
// Created by suhang on 20-8-2.
//
#include <tbb/concurrent_queue.h>
#include <io/tcp.h>
#include <atomic>

#ifndef NPOLLER_CONNECTION_POOL_H
#define NPOLLER_CONNECTION_POOL_H


class connection_pool {
public:
    explicit connection_pool(const std::function<std::shared_ptr<tcp>()>& _allocate_func =
            std::function<std::shared_ptr<tcp>()>([]() { return nullptr; }));

    void add_instance(std::shared_ptr<tcp> instance);

    std::shared_ptr<tcp> get_instance();

    virtual ~connection_pool();
    
private:
    tbb::concurrent_queue<std::shared_ptr<tcp>> free_pool;

    std::function<std::shared_ptr<tcp>()> allocate_tcp_func;

    std::atomic<int> size;

};


#endif //NPOLLER_CONNECTION_POOL_H
