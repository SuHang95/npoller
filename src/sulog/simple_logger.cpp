//
// Created by suhang on 19-5-3.
//

#include <fcntl.h>
#include <cstring>
#include <cstdarg>
#include <sys/time.h>
#include "simple_logger.h"

simple_logger::simple_logger(const std::string &name, log_level level, log_time_strategy name_strategy,
                             log_time_strategy log_strategy, bool sync) : name_time(log_time()) {
    this->name = name;
    this->name_strategy = name_strategy;
    this->log_strategy = log_strategy;
    this->is_sync = sync;
    this->level = level;

    open_file();
};

simple_logger::~simple_logger() {
    fclose();
}


char *simple_logger::new_format_str(const char *fmt, const std::string &time_str,
                                    const char *level, char *stack_addr,
                                    bool &use_stack, int &len) {
    size_t format_len = strlen(fmt);
    size_t time_len = time_str.size();
    size_t level_len = strlen(level);

    char *ret_val;
    len = strlen(fmt) + time_str.size() + strlen(level) + 2;

    if (len > FORMAT_SIZE_THRESHOLD) {
        ret_val = new char[len];
        use_stack = false;
    } else {
        ret_val = stack_addr;
        use_stack = true;
    }

    memcpy(ret_val, time_str.c_str(), time_len);
    memcpy(ret_val + time_len, level, level_len);
    memcpy(ret_val + time_len + level_len, fmt, format_len);
    ret_val[time_len + level_len + format_len] = '\n';
    ret_val[len - 1] = '\0';

    return ret_val;
}

void simple_logger::print(const char *level, const char *format, va_list args) {

    bool write_success = false, need_rename, use_stack_addr = true;
    int fmt_str_len = 0;
    const char *new_fmt_str = nullptr;
    std::string time_str = this->time(log_strategy, need_rename);

    reopen_if_need(need_rename);

    if (load_file_state() == open) {
        char stack_addr[FORMAT_SIZE_THRESHOLD];

        new_fmt_str = new_format_str(format, time_str, level,
                                     stack_addr, use_stack_addr, fmt_str_len);
        write_success = vfprintf(fmt_str_len, new_fmt_str, args);
    }

    if (!write_success) {
        reopen_if_need(true);
    }

    if (!use_stack_addr && new_fmt_str != nullptr) {
        delete[]new_fmt_str;
    }

    va_end(args);
}

void simple_logger::print(const char *level, const std::string &text) {
    bool write_success, need_rename;
    std::string time_str = this->time(log_strategy, need_rename);

    int count = 0;
    simple_logger::reopen_if_need(need_rename);
    if (load_file_state() == open) {
        count += ::fprintf(fp, "%s%s%s\n", time_str.c_str(), level, text.c_str());

        write_success &= (count >=
                          time_str.length() + strlen(level) + text.length() + 1);

        if (is_sync) {
            write_success &= (::fflush(fp) == EOF);
        }

    }

    if (!write_success) {
        simple_logger::reopen_if_need(true);
    }
}


void simple_logger::reopen_if_need(bool need_reopen) {
    if (need_reopen || load_file_state() == non_associated) {
        if (fp != NULL) {
            ::fclose(fp);
        }
        open_file();
    }
}

void simple_logger::open_file() {
    log_time now;

    std::string time_str = time(name_strategy, now);
    std::string file_name = name + time_str + ".log";
    fp = fopen(file_name.c_str(), "a+");
    if (fp != NULL) {
        set_file_state(open);
        set_name_time(now);
    } else {
        set_file_state(non_associated);
    }
}

std::string simple_logger::time(log_time_strategy strategy, bool &need_rename) {
    char buf[25] = {0};

    if (strategy == no) {
        return std::string{};
    }

    log_time now;

#ifdef _WIN32
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    switch (strategy) {
        case month:
            sprintf_s(buf, 25, "%4d-%02d", \
                sys.wYear, sys.wMonth);
            break;
        case day:
            sprintf_s(buf, 25, "%4d-%02d-%02d", \
                sys.wYear, sys.wMonth, sys.wDay);
            break;
        case hour:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d", \
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour);
            break;
        case minute:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d", \
                sys.wYear, sys.wMonth, , sys.wDay, sys.wHour, \
                sys.wMinute);
            break;
        case second:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d:%02d", \
                sys.wYear, sys.wMonth, , sys.wDay, sys.wHour, \
                sys.wMinute, sys.wSecond);
            break;
        case millisecond:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d:%02d-%03d", \
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour, \
                sys.wMinute, sys.wSecond, sys.wMilliseconds);
            break;
        default:
            return std::string{};
    }

    now.set_val(&sys);
#else
    struct tm *local;
#ifdef __unix__
    struct tm _local;
    local = &_local;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, local);
#elif
    time_t nowtime;
    nowtime = time(NULL);
    local = localtime(&nowtime);
#endif
    switch (strategy) {
        case month:
            strftime(buf, 25, "%Y-%m", local);
            break;
        case day:
            strftime(buf, 25, "%Y-%m-%d", local);
            break;
        case hour:
            strftime(buf, 25, "%Y-%m-%d-%H", local);
            break;
        case minute:
            strftime(buf, 25, "%Y-%m-%d-%H:%M", local);
            break;
        case second:
            strftime(buf, 25, "%Y-%m-%d-%H:%M:%S", local);
            break;
        case millisecond:
            strftime(buf, 25, "%Y-%m-%d-%H:%M:%S", local);
            break;
        default:
            return std::string{};
    }

    now.set_val(local);

    if (strategy == millisecond) {
#ifdef __unix__
        int millisecond = (tv.tv_usec / 1000) % 1000;
        char milli_str[10];
        sprintf(milli_str, "-%03d", millisecond);
        strcat(buf, milli_str);
#endif
    }

#endif

    if (load_name_time().is_init()) {
        need_rename = false;
    } else if (!load_name_time().equal(name_strategy, now)) {
        need_rename = true;
    }

    return std::string(buf, strlen(buf));
}

std::string simple_logger::time(log_time_strategy strategy, log_time &now) {
    char buf[25] = {0};

    if (strategy == no) {
        return std::string{};
    }

#ifdef _WIN32
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    switch (strategy) {
        case month:
            sprintf_s(buf, 25, "%4d-%02d", \
                sys.wYear, sys.wMonth);
            break;
        case day:
            sprintf_s(buf, 25, "%4d-%02d-%02d", \
                sys.wYear, sys.wMonth, sys.wDay);
            break;
        case hour:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d", \
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour);
            break;
        case minute:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d", \
                sys.wYear, sys.wMonth, , sys.wDay, sys.wHour, \
                sys.wMinute);
            break;
        case second:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d:%02d", \
                sys.wYear, sys.wMonth, , sys.wDay, sys.wHour, \
                sys.wMinute, sys.wSecond);
            break;
        case millisecond:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d:%02d-%03d", \
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour, \
                sys.wMinute, sys.wSecond, sys.wMilliseconds);
            break;
        default:
            return std::string{};
    }

    now.set_val(&sys);
#else
    struct tm *local;
#ifdef __unix__
    struct tm _local;
    local = &_local;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, local);
#elif
    time_t nowtime;
    nowtime = time(NULL);
    local = localtime(&nowtime);
#endif
    switch (strategy) {
        case month:
            strftime(buf, 25, "%Y-%m", local);
            break;
        case day:
            strftime(buf, 25, "%Y-%m-%d", local);
            break;
        case hour:
            strftime(buf, 25, "%Y-%m-%d-%H", local);
            break;
        case minute:
            strftime(buf, 25, "%Y-%m-%d-%H:%M", local);
            break;
        case second:
            strftime(buf, 25, "%Y-%m-%d-%H:%M:%S", local);
            break;
        case millisecond:
            strftime(buf, 25, "%Y-%m-%d-%H:%M:%S", local);
            break;
        default:
            return std::string{};
    }

    now.set_val(local);

    if (strategy == millisecond) {
#ifdef __unix__
        int millisecond = (tv.tv_usec / 1000) % 1000;
        char milli_str[10];
        sprintf(milli_str, "-%03d", millisecond);
        strcat(buf, milli_str);
#endif
    }

#endif

    return std::string(buf, strlen(buf));
}

std::string simple_logger::time(log_time_strategy strategy) {
    char buf[25] = {0};

    if (strategy == no) {
        return std::string{};
    }

#ifdef _WIN32
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    switch (strategy) {
        case month:
            sprintf_s(buf, 25, "%4d-%02d", \
                sys.wYear, sys.wMonth);
            break;
        case day:
            sprintf_s(buf, 25, "%4d-%02d-%02d", \
                sys.wYear, sys.wMonth, sys.wDay);
            break;
        case hour:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d", \
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour);
            break;
        case minute:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d", \
                sys.wYear, sys.wMonth, , sys.wDay, sys.wHour, \
                sys.wMinute);
            break;
        case second:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d:%02d", \
                sys.wYear, sys.wMonth, , sys.wDay, sys.wHour, \
                sys.wMinute, sys.wSecond);
            break;
        case millisecond:
            sprintf_s(buf, 25, "%4d-%02d-%02d-%02d:%02d:%02d-%03d", \
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour, \
                sys.wMinute, sys.wSecond, sys.wMilliseconds);
            break;
        default:
            return std::string{};
    }

#else
    struct tm *local;
#ifdef __unix__
    struct tm _local;
    local = &_local;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, local);
#elif
    time_t nowtime;
    nowtime = time(NULL);
    local = localtime(&nowtime);
#endif
    switch (strategy) {
        case month:
            strftime(buf, 25, "%Y-%m", local);
            break;
        case day:
            strftime(buf, 25, "%Y-%m-%d", local);
            break;
        case hour:
            strftime(buf, 25, "%Y-%m-%d-%H", local);
            break;
        case minute:
            strftime(buf, 25, "%Y-%m-%d-%H:%M", local);
            break;
        case second:
            strftime(buf, 25, "%Y-%m-%d-%H:%M:%S", local);
            break;
        case millisecond:
            strftime(buf, 25, "%Y-%m-%d-%H:%M:%S", local);
            break;
        default:
            return std::string{};
    }

    if (strategy == millisecond) {
#ifdef __unix__
        int millisecond = (tv.tv_usec / 1000) % 1000;
        char milli_str[10];
        sprintf(milli_str, "-%03d", millisecond);
        strcat(buf, milli_str);
#endif
    }

#endif

    return std::string(buf, strlen(buf));
}