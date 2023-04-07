#ifndef __io_buffer_h__
#define __io_buffer_h__

#include <spinlock/spin_lock.h>
#include <cassert>
#include "buffer.h"
#include <shared_mutex>

class io_buffer {
public:
    io_buffer();

    io_buffer(const io_buffer &other) = default;

    io_buffer &operator=(const io_buffer &other) = default;

    virtual ~io_buffer() {}

    void push_back_n(const char *src, size_t size);

    void pop_back_n(size_t size);

    size_t pop_back_n(char *dest,size_t size);

    void pop_front_n(size_t size);

    size_t pop_front_n(char *dest,size_t size);

    void push_front_n(const char *src, size_t size);

    char get(size_t n);

    void pop_back_to_other_front_n(io_buffer &other, size_t n);

    void pop_front_to_other_back_n(io_buffer &other, size_t n);

    //not checked
    size_t get_n(size_t n, char *dest, size_t size);

    void set(size_t n, char a);

    size_t size() const;

    void shrink_size();

    inline std::string test_info() const;

    class page;

protected:
    //the function below is the nonlock inline func,is to used in other member function or the friend class,function
    inline void __shrink_size();

    inline size_t __size() const;

    inline void check() const;

    void self_pop_back_to_front_n(size_t n);

    void other_pop_back_to_front_n(io_buffer &other, size_t n);

    void self_pop_front_to_back_n(size_t n);

    void other_pop_front_to_back_n(io_buffer &other, size_t n);

    static mem_container mem;

    struct control_block {
        std::deque<char *> _buffer;
        size_t begin;
        size_t end;
    };

    std::shared_ptr<control_block> block_ptr;


#ifdef IO_H
    friend class io;
    friend class tcp;
#endif

private:
    inline size_t &begin() {
        return block_ptr->begin;
    }

    inline size_t &end() {
        return block_ptr->end;
    }

    inline std::deque<char *> &_buffer() {
        return block_ptr->_buffer;
    }
};


//the function below is the nonlock inline func,is to used in other member function or the friend class,function
inline void io_buffer::__shrink_size() {
    check();
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    if (begin == (size_t) -1) {
        while (!_buffer.empty()) {
            mem.push(_buffer[0]);
            _buffer.pop_front();
        }
    } else {
        while ((begin / DEFAULT_SIZE) > 0) {
            mem.push(_buffer[0]);
            begin -= DEFAULT_SIZE;
            end -= DEFAULT_SIZE;
            _buffer.pop_front();
        }

        while ((_buffer.size()) >
               (end / DEFAULT_SIZE) + 1) {
            mem.push(_buffer.back());
            _buffer.pop_back();
        }
    }
}


inline size_t io_buffer::__size() const {
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;


    if (begin == ((size_t) -1) || end == 0) {
        return 0;
    }
    return (end - begin);
}

inline void io_buffer::check() const {
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    if (begin == size_t(-1) || end == 0 || begin >= end) {
        begin = size_t(-1);
        end = 0;
        return;
    }
    if ((end - begin) > (_buffer.size()) * DEFAULT_SIZE) {
#ifdef _DEBUG
        std::cout << "The begin=" << begin << ",the end=" << end << ",io_buffer size=" << _buffer.size()
                  << std::endl;
#endif
        throw std::logic_error("io_buffer overflow!");
    }
}

inline std::string io_buffer::test_info() const {
    char result[60];
    sprintf(result, "Begin=%lu,End=%lu,size of io_buffer=%lu", block_ptr->begin,
            block_ptr->end, block_ptr->_buffer.size());
    return std::string(result);
}

class io_buffer::page {
private:
    char *ptr;
    std::atomic<int> count;
    static size_t page_size;
    std::shared_mutex mutex_;
public:
    inline char &operator[](size_t n) {
#ifdef _DEBUG
        assert(n >= page_size);
#endif
        return ptr[n];
    }

    inline int add_count() {
        return count.fetch_add(1);
    }

    inline int sub_count() {
        int ret_val = count.fetch_add(-1);
#ifdef _DEBUG
        assert(ret_val >= 1);
#endif
        return ret_val;
    }
};


#endif
