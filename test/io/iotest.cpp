#include<io/event_processor.h>
#include<io/io.h>
#include<io/io_factory.h>

class print_op : public io_op {
public:
    print_op(std::string &&str, task *_task, int _io_id) : print_str(str),
                                                           _task(_task) {
        io_id = _io_id;
        type = Write;
    }

    //for write operation, must return true
    virtual bool process(io_buffer &buffer) {
        buffer.push_back_n(print_str.c_str(), print_str.size());
        return true;
    }

    virtual void notify(result_type result) {
        _task->notify(*this, result);
    }

    //only used for write event;
    virtual size_t size() {
        return print_str.size();
    }

    virtual const char *fail_message() {
        return "";
    };

private:
    std::string print_str;
    task *_task;
};


class print_task : task {
public:
    print_task(event_processor &__processor, logger __log) :
            processor(__processor), log(__log) {
        op = std::make_shared<print_op>("Hello World!", dynamic_cast<task *>(this), 1);
    }

    void run() {
        if (state == 0) {
            io::io_type type;

            std::packaged_task<void(std::shared_ptr<io> &&, const std::string &)> callback(
                    [this](std::shared_ptr<io> &&ptr, const std::string &message) {
                        log.info("Get io instance, which id is:%d", ptr->id());
                        if (ptr != nullptr && ptr->do_register(std::dynamic_pointer_cast<io_op>(op))) {
                            this->state = 1;
                        } else {
                            this->state = 2;
                            this->log.info("print task can not add in io out!");
                        }
                    });

            //callback mode
            processor.get_factory().create_io_with_callback<io>(std::move(callback), 1, io::writable, false,
                                                                processor.get_logger());

            return;
        }
        if (state == 1) {
            log.info("Print task done!");
            state = 2;
            return;
        }
        if (state == 2) {
            log.info("Task finished!");
            return;
        }
    }

    void notify(io_op &ev, io_op::result_type result) {
        if (result == io_op::Done) {
            state = 1;
            run();
        } else {
            log.error("Some error occur,print task fail!");
            state = 2;
            run();
        }
    }

private:
    event_processor &processor;
    logger log;
    std::shared_ptr<io_op> op;
};

int main() {
    logger log("logfile", logger::DEBUG, true);
    event_processor manager(log);
    print_task hello(manager, log);
    hello.run();
    pause();
    return 0;
}



