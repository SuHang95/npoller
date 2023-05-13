#ifndef __task_h__
#define __task_h__

#include<iobuffer/io_buffer.h>
#include<map>
#include<functional>
#include<sys/time.h>

class io_buffer;

class io_op {
public:
    io_op() {}

    uint64_t id;

    enum operation_type : unsigned char {
        Read = 0x0,
        Write = 0x01
    } type;

    enum result_type : unsigned char {
        //WriteFlush = 0x01,
        Done = 0x02,
        IOError = 0x03,
        IOClose = 0x04
    } result;

    const static int unspecified_id = -1;

    const static size_t unspecified_size = -1;

    int io_id;

    //for write event,it must return true
    virtual bool process(io_buffer &) = 0;

    virtual void notify(result_type result) = 0;

    //only used for write event;
    virtual size_t size() = 0;

    virtual const char *fail_message() = 0;
};


//this class is to implement the state machine
class task {
public:
    task() : state(0) {};

    task(const task &);

    task(task &&);

    task &operator=(const task &) = delete;

    task &operator=(task &&) = delete;

    virtual void notify(io_op &io, io_op::result_type result) = 0;

protected:
    int state;
};


#endif
