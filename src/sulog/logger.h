#ifndef _LOGGER_H__
#define _LOGGER_H__

#include "simple_logger.h"
#include "base_logger.h"
#include "spinlock/spin_lock.h"
#include <mutex>

//a logger which can access by multi thread,but can't be copied
class logger : public base_logger {
public:
    explicit inline logger(const std::string &name, log_level level, bool sync = false,
                           log_time_strategy name_strategy = day,
                           log_time_strategy log_strategy = millisecond);

    explicit inline logger();

    inline logger(const logger &);

    inline logger &operator=(const logger &);

    std::string time(log_time_strategy);


    virtual inline void error(const char *format, ...) final;

    virtual inline void error(const std::string &) final;

    virtual inline void warn(const char *format, ...) final;

    virtual inline void warn(const std::string &) final;

    virtual inline void info(const char *format, ...) final;

    virtual inline void info(const std::string &) final;

    virtual inline void debug(const char *format, ...) final;

    virtual inline void debug(const std::string &) final;

    virtual inline bool is_debug_enable() const;

    inline std::string name() {
        return log->name;
    }

    friend void swap(logger &first, logger &second);



private:
    class internal_logger : public simple_logger {
    public:
        explicit internal_logger(const std::string &name, logger::log_level level, bool sync = false,
                                 log_time_strategy name_strategy = day,
                                 log_time_strategy log_strategy = millisecond);

        inline explicit internal_logger() : simple_logger() {}

        virtual ~internal_logger();

    private:
        std::mutex _mutex;

        friend class logger;

    protected:
        virtual inline void reopen_if_need(bool need_reopen);

        virtual inline void print(const char *, const std::string &);

        virtual inline bool fprintf(int, const char *format...);

        virtual inline bool vfprintf(int, const char *foramt, va_list args);

        virtual inline int fclose();

        virtual inline file_state load_file_state();

        virtual inline void set_file_state(file_state state);

        virtual inline logger::log_time load_name_time();

        virtual inline void set_name_time(logger::log_time time);


    };

    reentrant_spin_lock lock;
    std::shared_ptr<internal_logger> log;

    static std::shared_ptr<internal_logger> empty_logger;
};


base_logger::log_time logger::internal_logger::load_name_time() {
    return name_time.load(std::memory_order_relaxed);
}

void logger::internal_logger::set_name_time(logger::log_time time) {
    name_time.store(time, std::memory_order_relaxed);
}

simple_logger::file_state logger::internal_logger::load_file_state() {
    return state.load(std::memory_order_relaxed);
}

void logger::internal_logger::set_file_state(simple_logger::file_state state) {
    this->state.store(state, std::memory_order_relaxed);
}

bool logger::internal_logger::fprintf(int count, const char *format...) {
    va_list args;
    va_start(args, format);
    bool succ;

    {
        std::unique_lock<std::mutex> _g(this->_mutex);
        succ = simple_logger::vfprintf(count, format, args);
    }

    va_end(args);

    return succ;
}

bool logger::internal_logger::vfprintf(int length, const char *format, va_list args) {
    std::unique_lock<std::mutex> _g(this->_mutex);
    return simple_logger::vfprintf(length, format, args);
}

int logger::internal_logger::fclose() {
    std::unique_lock<std::mutex> _g(this->_mutex);
    return simple_logger::fclose();
}

void logger::internal_logger::reopen_if_need(bool need_reopen) {
    if (need_reopen || load_file_state() == non_associated) {
        std::unique_lock<std::mutex> _g(this->_mutex);
        return simple_logger::reopen_if_need(need_reopen);
    }
}

void logger::internal_logger::print(const char *level, const std::string &text) {
    std::unique_lock<std::mutex> _g(this->_mutex);
    return simple_logger::print(level, text);
}


logger::logger(const std::string &name, log_level level, bool sync,
               log_time_strategy name_strategy, log_time_strategy log_strategy) :
        log(std::make_shared<internal_logger>(name, level, sync,
                                              name_strategy, log_strategy)) {}

logger::logger() : log(empty_logger) {}

logger::logger(const logger &that) {
    std::unique_lock<reentrant_spin_lock> t(const_cast<reentrant_spin_lock &>(that.lock));
    log = that.log;
}

logger &logger::operator=(const logger &that) {
    if (this == &that) {
        return *this;
    }
    logger *first = (this < (&that)) ? this : const_cast<logger *> (&that);
    logger *second = this > &that ? this : const_cast<logger *> (&that);

    {
        std::unique_lock<reentrant_spin_lock> t(first->lock);
        {
            std::unique_lock<reentrant_spin_lock> t1(second->lock);
            this->log = that.log;
        }
    }
    return *this;
}

void logger::debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log->simple_logger::debug(format, args);
    va_end(args);
}

void logger::debug(const std::string &text) {
    log->simple_logger::debug(text);
}

void logger::info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log->simple_logger::info(format, args);
    va_end(args);
}

void logger::info(const std::string &text) {
    log->simple_logger::info(text);
}

void logger::warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log->simple_logger::warn(format, args);
    va_end(args);
}

void logger::warn(const std::string &text) {
    log->simple_logger::warn(text);
}


void logger::error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log->simple_logger::error(format, args);
    va_end(args);
}

void logger::error(const std::string &text) {
    log->simple_logger::error(text);
}

bool logger::is_debug_enable() const {
    return log->is_debug_enable();
}

inline std::string logger::time(log_time_strategy strategy) {
    return log->time(strategy);
}


inline void swap(logger &first, logger &second) {
    logger *logger1 = &first < &second ? &first : const_cast<logger *> (&second);
    logger *logger2 = &first > &second ? &first : const_cast<logger *> (&second);

    {
        std::unique_lock<reentrant_spin_lock> t(logger1->lock);
        {
            std::unique_lock<reentrant_spin_lock> t1(logger2->lock);
            std::swap(logger1->log,logger2->log);
            //std::shared_ptr<logger::internal_logger> tmp = logger1->log;
            //logger1->log = logger2->log;
            //logger2->log = tmp;
        }
    }
}

#endif
