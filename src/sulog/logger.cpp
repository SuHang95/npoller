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


