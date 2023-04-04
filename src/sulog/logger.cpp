//
// Created by suhang on 19-11-1.
//

#include "logger.h"

std::shared_ptr<logger::internal_logger> logger::empty_logger =
        std::make_shared<logger::internal_logger>();


logger::internal_logger::~internal_logger(){
    std::unique_lock<std::mutex> _g(this->_mutex);
    this->simple_logger::~simple_logger();
}

logger::internal_logger::internal_logger(const std::string &name, logger::log_level level, bool sync,
                                         log_time_strategy name_strategy, log_time_strategy log_strategy)
        : simple_logger() {
    std::unique_lock<std::mutex> _g(this->_mutex);

    this->name = name;
    this->name_strategy = name_strategy;
    this->log_strategy = log_strategy;
    this->is_sync = sync;
    this->level = level;

    open_file();
}
