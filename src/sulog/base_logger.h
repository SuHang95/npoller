//
// Created by suhang on 19-5-18.
//

#ifndef NETPROCESSOR_BASE_LOGGER_H
#define NETPROCESSOR_BASE_LOGGER_H

enum log_time_strategy {
    no = 0,
    month = 10,
    day = 9,
    hour = 8,
    minute = 7,
    second = 6,
    millisecond = 5
};

class base_logger {
public:
    enum log_level {
        DEBUG = 0,
        INFO,
        WARN,
        ERROR,
        NO
    };


    virtual void error(const char *format, ...) = 0;

    virtual void error(const std::string &) = 0;

    virtual void warn(const char *format, ...) = 0;

    virtual void warn(const std::string &) = 0;

    virtual void info(const char *format, ...) = 0;

    virtual void info(const std::string &) = 0;

    virtual void debug(const char *format, ...) = 0;

    virtual void debug(const std::string &) = 0;

    virtual bool is_debug_enable() const = 0;

    std::string time(log_time_strategy);

protected:

    //时间的值，精确到name_strategy中的时间单位
    class log_time {
    public:
        log_time() {
            year = 0;
            month = 0;
            date = 0;
            hour = 0;
            init = 0;
        }

#ifdef _WIN32
        inline void set_val(SYSTEMTIME *sys) {
            init = 1;
            year = sys->wYear;
            month = sys->wMonth;
            date = sys->wDay;
            hour = sys->wHour;
        }

#elif __unix__

        inline void set_val(struct tm *time) {
            init = 1;
            year = time->tm_year;
            month = time->tm_mon;
            date = time->tm_mday;
            hour = time->tm_hour;
#ifdef _DEBUG
            minute = time->tm_min;
#endif
        }

#endif

        inline bool is_init() {
            return init == 0;
        }

        inline bool equal(log_time_strategy strategy, log_time other) {
            bool result = true;
            result &= (strategy > month) || (year == other.year && month == other.month);
            result &= (strategy > day) || (date == other.date);
            result &= (strategy > hour) || (hour == other.hour);
#ifdef _DEBUG
            result &= (strategy > minute) || (minute == other.minute);
#endif
            return result;
        }

    private:
        unsigned int year:12;
        unsigned int month:4;
        unsigned int date:5;
        unsigned int hour:5;
#ifdef _DEBUG
        unsigned int minute:6;
#endif
        unsigned int init:1;
    };

};

#endif //NETPROCESSOR_BASE_LOGGER_H