//
// Created by suhang on 19-5-3.
//

#ifndef NETPROCESSOR_SIMPLE_LOGGER_H
#define NETPROCESSOR_SIMPLE_LOGGER_H

#include <string>
#include "base_logger.h"
#include <cstdarg>
#include <atomic>
#include <cstring>
#include <memory>
#include <iostream>

class simple_logger : base_logger {
public:

    explicit simple_logger(const std::string &name, log_level level, bool sync = false,
                           log_time_strategy name_strategy = day,
                           log_time_strategy log_strategy = millisecond);

    inline explicit simple_logger();

    simple_logger(const simple_logger &) = delete;

    simple_logger &operator=(const simple_logger &) = delete;

    std::string time(log_time_strategy strategy);


    inline void error(const char *format, ...) final;

    inline void error(const std::string &text) final {
        print(" ERROR ", text);
    };

    inline void error(const char *format, va_list args);

    inline void warn(const char *format, ...) final;

    inline void warn(const std::string &text) final {
        if (level <= WARN) {
            print(" WARN ", text);
        }
    }

    inline void warn(const char *format, va_list args);

    inline void info(const char *format, ...) final;


    inline void info(const std::string &text) final {
        if (level <= INFO) {
            print(" INFO ", text);
        }
    }

    inline void info(const char *format, va_list args);

    inline void debug(const char *format, ...) final;

    inline void debug(const std::string &text) final {
        if (level <= DEBUG) {
            print(" DEBUG ", text);
        }
    }

    inline void debug(const char *format, va_list args);

    inline bool is_debug_enable() const {
        return level <= DEBUG;
    }

    virtual ~simple_logger();

protected:
    enum file_state {
        open,   //文件处于打开状态，fp指针有效
        non_associated,  //fp指针无效，不关联任何文件描述符
        bad_file_descriptor //需要手动关闭
    };

    std::atomic<file_state> state;

    std::atomic<log_time> name_time;

    //log name
    std::string name;
    //name strategy
    log_time_strategy name_strategy;
    //log strategy
    log_time_strategy log_strategy;
    //log mode
    bool is_sync;
    //log level
    log_level level;

    virtual void reopen_if_need(bool need_reopen);

    void open_file();

    virtual void print(const char *, const char *, va_list args);

    virtual void print(const char *, const std::string &);


    std::string time(log_time_strategy strategy, bool &need_rename);

    std::string time(log_time_strategy strategy, log_time &now);

    virtual inline bool fprintf(int, const char *format...);

    virtual inline bool vfprintf(int, const char *format, va_list args);

    virtual inline int fclose();

    virtual inline file_state load_file_state();

    virtual inline void set_file_state(file_state state);

    virtual inline log_time load_name_time();

    virtual inline void set_name_time(log_time time);

private:
    //文件指针
    FILE *fp;

    //以下为实现中所需要的常量

    //格式字符串长度阈值，超出此阈值则使用堆上空间存放实际写进磁盘的内容
    static const int FORMAT_SIZE_THRESHOLD = 512;

    //申请的用于存放新format串的内存地址大小/格式字符串大小
    static const int REAL_SIZE_FORMAT_TIMES = 3;

    //如果申请的字符串大小不足以存放最终打印到文件中的，则下次申请的长度倍数
    static const int MALLOC_SIZE_PRE_TIMES = 4;


    /**
     * @param fmt               the former format str
     * @param time_str
     * @param level
     * @param stack_addr        use fixed-size array to avoid malloc and free
     * @param use_stack         whether return value is the stack_addr,if not
     *                          the user must delete this pointer
     * @param len               the length of the new format string
     **/
    static char *new_format_str(const char *fmt, const std::string &time_str,
                                const char *level, char *stack_addr,
                                bool &use_stack, int &len);


};

simple_logger::simple_logger() :
        name_time(log_time()),
        state(file_state()), name(std::string()),
        name_strategy(no), level(NO) {}


bool simple_logger::fprintf(int count, const char *format...) {
    va_list args;
    va_start(args, format);

    bool succ = (::vfprintf(fp, format, args) >= count);
    if (is_sync) {
        succ &= (::fflush(fp) == EOF);
    }
    return succ;
}

bool simple_logger::vfprintf(int length, const char *format, va_list args) {
    int ret = ::vfprintf(fp, format, args);
    //we can not know how much byte we actually need write
    bool succ = (ret > 0);

    if (is_sync) {
        succ &= (::fflush(fp) == 0);
    }
    if (errno == EBADF) {
        this->set_file_state(bad_file_descriptor);
    }
    return succ;
}

int simple_logger::fclose() {
    auto ret = (fp == nullptr ? 0 : (::fclose(fp)));
    fp = nullptr;
    return ret;
}

simple_logger::file_state simple_logger::load_file_state() {
    return *(reinterpret_cast<simple_logger::file_state *>(&this->state));
}

void simple_logger::set_file_state(simple_logger::file_state state) {
    *(reinterpret_cast<simple_logger::file_state *>(&this->state)) = state;
}

base_logger::log_time simple_logger::load_name_time() {
    return *(reinterpret_cast<log_time *>(&name_time));
}

void simple_logger::set_name_time(base_logger::log_time time) {
    *(reinterpret_cast<log_time *>(&name_time)) = time;
}


void simple_logger::debug(const char *format, ...) {
    if (this->level > DEBUG) {
        return;
    }

    va_list args;
    va_start(args, format);
    print(" DEBUG ", format, args);
    va_end(args);
}


void simple_logger::info(const char *format, ...) {
    if (this->level > INFO) {
        return;
    }

    va_list args;
    va_start(args, format);
    print(" INFO ", format, args);
    va_end(args);
}

void simple_logger::warn(const char *format, ...) {
    if (this->level > WARN) {
        return;
    }

    va_list args;
    va_start(args, format);
    print(" WARN ", format, args);
    va_end(args);
}

void simple_logger::error(const char *format, ...) {
    if (this->level > ERROR) {
        return;
    }

    va_list args;
    va_start(args, format);
    print(" ERROR ", format, args);
    va_end(args);
}

void simple_logger::debug(const char *format, va_list args) {
    if (this->level > DEBUG) {
        return;
    }
    print(" DEBUG ", format, args);
}


void simple_logger::info(const char *format, va_list args) {
    if (this->level > INFO) {
        return;
    }
    print(" INFO ", format, args);
}

void simple_logger::warn(const char *format, va_list args) {
    if (this->level > WARN) {
        return;
    }
    print(" WARN ", format, args);
}


void simple_logger::error(const char *format, va_list args) {
    if (this->level > ERROR) {
        return;
    }
    print(" ERROR ", format, args);
}

#endif //NETPROCESSOR_SIMPLE_LOGGER_H
