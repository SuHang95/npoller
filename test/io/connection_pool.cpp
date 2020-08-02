//
// Created by suhang on 20-8-2.
//

#include "connection_pool.h"

connection_pool::connection_pool(const std::function<std::shared_ptr<tcp>()> &_allocate_func) : size(0),
                                                                                                allocate_tcp_func(
                                                                                                        _allocate_func) {}

void connection_pool::add_instance(std::shared_ptr<tcp> instance) {
    free_pool.push(instance);
    size.fetch_add(1, std::memory_order::memory_order_relaxed);
}

std::shared_ptr<tcp> connection_pool::get_instance() {
    std::shared_ptr<tcp> pop_item;

    if (free_pool.try_pop(pop_item)) {
        size.fetch_add(-1, std::memory_order_relaxed);
        return pop_item;
    }

    pop_item = allocate_tcp_func();

    if (pop_item == nullptr) {
        return nullptr;
    }

    return pop_item;
}

connection_pool::~connection_pool() {
    std::shared_ptr<tcp> pop_item;

    while (free_pool.try_pop(pop_item)) {
        pop_item->close();
    }
}