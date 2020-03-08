#include"io_buffer.h"

//size_t io_buffer::page::page_size = defaultsize;

mem_container io_buffer::mem(0x40000000);


io_buffer::io_buffer() :
        block_ptr(std::make_shared<control_block>()) {
    block_ptr->begin = -1;
    block_ptr->end = 0;
}

void io_buffer::push_back_n(const char *src, size_t size) {
    std::unique_lock<std::mutex> protect(block_ptr->mutex_for_data);
    check();
    if (size == 0 || src == NULL) {
        return;
    }

    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    if (begin == (size_t) -1) {
        /*如果buffer为空，因为size不为0，所以现在把begin置0，为确保end正常，
        也置0，但这事实上是一个不太正常的状态，需要注意*/
        begin = 0;
        end = 0;
    }

    while ((size + (end)) > (_buffer.size() * DEFAULT_SIZE)) {
        char *new_memory = mem.get();
        _buffer.push_back(new_memory);
    }

    char *this_block_ptr = _buffer[end / DEFAULT_SIZE];
    for (size_t i = 0; i < size; i++) {
        if ((end + i) % DEFAULT_SIZE == 0)
            this_block_ptr = _buffer[(end + i) / DEFAULT_SIZE];
        this_block_ptr[(i + end) % DEFAULT_SIZE] = src[i];
    }
    end += size;
}

void io_buffer::pop_back_n(size_t size) {
    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);

    check();
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;


    if (begin == (size_t) -1)
        return;
        //注意，end-begin是队列现在实际的size
    else if (size > end - begin) {
        begin = (size_t) -1;
        end = 0;
    } else {
        end -= size;
    }

    check();
    __shrink_size();

}

size_t io_buffer::pop_back_n(char *dest, size_t size) {
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);
    if ((begin == (size_t) -1) && (begin >= end)) {
        begin = -1;
        end = 0;
        return 0;
    }

    size_t n = end - begin >= size ? end - size : 0;
    size = end - begin >= size ? size : end - begin;

    if (n >= (end - begin)) {
        return 0;
    }
    if ((begin + n + size - 1) >= end) {
        size = end - (begin + n);
    }
    char *this_block_ptr = _buffer[(begin + n) / DEFAULT_SIZE];
    for (size_t i = 0; i < size; i++) {
        if ((begin + n + i) % DEFAULT_SIZE == 0)
            this_block_ptr = _buffer[(begin + n + i) / DEFAULT_SIZE];
        dest[i] = this_block_ptr[(begin + n + i) % DEFAULT_SIZE];
        //dest[i] = _buffer[(begin + n + i) / defaultsize][(begin + n + i) % defaultsize];
    }

    end -= size;

    if (begin >= end) {
        begin = -1;
        end = 0;
    }

    __shrink_size();

    return size;
}

void io_buffer::pop_front_n(size_t size) {
    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);

    check();
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;


    if (begin == (size_t) -1) {
        return;
    } else if (size > (end - begin)) {
        begin = (size_t) -1;
        end = 0;
    } else {
        begin += size;
    }

    __shrink_size();
}

size_t io_buffer::pop_front_n(char *dest, size_t size) {
    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    if ((begin == (size_t) -1) && (begin >= end)) {
        begin = -1;
        end = 0;
        return 0;
    }
    if ((begin + size - 1) >= end) {
        size = end - begin;
    }
    char *this_block_ptr = _buffer[begin / DEFAULT_SIZE];
    for (size_t i = 0; i < size; i++) {
        if ((begin + i) % DEFAULT_SIZE == 0)
            this_block_ptr = _buffer[(begin + i) / DEFAULT_SIZE];
        dest[i] = this_block_ptr[(begin + i) % DEFAULT_SIZE];
        //dest[i] = _buffer[(begin + i) / DEFAULT_SIZE][(begin + i) % DEFAULT_SIZE];
    }

    begin += size;

    if ((begin >= end)) {
        begin = -1;
        end = 0;
    }

    __shrink_size();

    return size;
}


void io_buffer::push_front_n(const char *src, size_t size) {
    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);
    if (size == 0 || src == nullptr) {
        return;
    }
    check();

    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;



    /*如空间不足申请空间，否则直接复制*/
    if (begin == (size_t) -1) {
        begin = 0;
        end = 0;
        while (size > (_buffer.size() * DEFAULT_SIZE)) {
            char *new_buff = mem.get();
            _buffer.push_front(new_buff);
        }
        end += size;
    } else {
        while (size > begin) {
            char *new_buff = mem.get();
            _buffer.push_front(new_buff);
            begin += DEFAULT_SIZE;
            end += DEFAULT_SIZE;
        }
        begin -= size;
    }
    char *block_to_write = _buffer[begin / DEFAULT_SIZE];
    for (size_t i = 0; i < size; i++) {
        if ((begin + i) % DEFAULT_SIZE == 0) {
            block_to_write = _buffer[(begin + i) / DEFAULT_SIZE];
        }
        block_to_write[(begin + i) % DEFAULT_SIZE] = src[i];
        //_buffer[(begin+i) / defaultsize][(begin + i) % defaultsize] = src[i];
    }
}


char io_buffer::get(size_t n) {

    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);
    if (n >= (end - begin) || begin == (size_t) -1) {
        throw std::logic_error("index out of bounds!");
    } else {
        return _buffer[(begin + n) / DEFAULT_SIZE][(begin + n) % DEFAULT_SIZE];
    }
}

//not checked
size_t io_buffer::get_n(size_t n, char *dest, size_t size) {

    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);
    if ((begin == (size_t) -1) && (begin >= end)) {
        begin = -1;
        end = 0;
        return 0;
    }
    if (n >= (end - begin)) {
        return 0;
    }
    if ((begin + n + size - 1) >= end) {
        size = end - (begin + n);
    }
    char *this_block_ptr = _buffer[(begin + n) / DEFAULT_SIZE];
    for (size_t i = 0; i < size; i++) {
        if ((begin + n + i) % DEFAULT_SIZE == 0)
            this_block_ptr = _buffer[(begin + n + i) / DEFAULT_SIZE];
        dest[i] = this_block_ptr[(begin + n + i) % DEFAULT_SIZE];
        //dest[i] = _buffer[(begin + n + i) / defaultsize][(begin + n + i) % defaultsize];
    }
    return size;
}

void io_buffer::set(size_t n, char a) {
    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);

    size_t &begin = block_ptr->begin;
    size_t &end = block_ptr->end;
    std::deque<char *> &_buffer = block_ptr->_buffer;

    if (n > (end - begin)) {
        throw std::logic_error("Buffer Overflow!");
    } else {
        _buffer[(begin + n) / DEFAULT_SIZE][(begin + n) % DEFAULT_SIZE] = a;
    }
}

size_t io_buffer::size() const {
    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);
    return __size();
}

void io_buffer::shrink_size() {
    std::lock_guard<std::mutex> protect(block_ptr->mutex_for_data);
    __shrink_size();
}


void io_buffer::pop_back_to_other_front_n(io_buffer &other, size_t n) {
    //if this two instance point to same io_buffer in fact
    if (block_ptr == other.block_ptr) {
        std::lock_guard<std::mutex> m(block_ptr->mutex_for_data);
        self_pop_back_to_front_n(n);
    }

        //to compare the pointer,we get the order of the mutex lock,avoid the deadlock
    else if ((size_t) block_ptr.get() < (size_t) other.block_ptr.get()) {
        std::lock_guard<std::mutex> m(block_ptr->mutex_for_data);
        {
            std::lock_guard<std::mutex> m1(other.block_ptr->mutex_for_data);
            other_pop_back_to_front_n(other, n);
        }
    } else {
        std::lock_guard<std::mutex> m(other.block_ptr->mutex_for_data);
        {
            std::lock_guard<std::mutex> m1(block_ptr->mutex_for_data);
            other_pop_back_to_front_n(other, n);
        }
    }
}

void io_buffer::pop_front_to_other_back_n(io_buffer &other, size_t n) {

    //if this two instance point to same io_buffer in fact
    if (block_ptr == other.block_ptr) {
        std::lock_guard<std::mutex> m(mutex_for_data());
        self_pop_front_to_back_n(n);
    }

        //to compare the pointer,we get the order of the mutex lock,avoid the deadlock
    else if ((size_t) block_ptr.get() < (size_t) other.block_ptr.get()) {
        std::lock_guard<std::mutex> m(mutex_for_data());
        {
            std::lock_guard<std::mutex> m2(other.mutex_for_data());
            other_pop_front_to_back_n(other, n);
        }
    } else {
        std::lock_guard<std::mutex> m(other.mutex_for_data());

        {
            std::lock_guard<std::mutex> m2(mutex_for_data());
            other_pop_front_to_back_n(other, n);
        }
    }
}


void io_buffer::self_pop_back_to_front_n(size_t n) {
    check();
    if ((begin() == (size_t) -1) || (n == 0) || (n >= (end() - begin()))) {
        return;
    }

    if (((end() - begin()) % DEFAULT_SIZE == 0) && (n > 2 * DEFAULT_SIZE)) {
        int last = (end() - 1) % DEFAULT_SIZE;

        n -= ((last + 1) < DEFAULT_SIZE ? (last + 1) : 0);
        char *first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
        char *last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
        while (last >= 0 && last < DEFAULT_SIZE - 1) {
            begin()--;
            first_block_ptr[begin() % DEFAULT_SIZE] =
                    last_block_ptr[(end() - 1) % DEFAULT_SIZE];
            last--;
            end()--;
        }

        while (n > DEFAULT_SIZE) {
            if (begin() != 0) {
                char *temp = _buffer()[(begin() - 1) / DEFAULT_SIZE];
                _buffer()[(begin() - 1) / DEFAULT_SIZE] = _buffer()[(end() - 1) / DEFAULT_SIZE];
                _buffer()[(end() - 1) / DEFAULT_SIZE] = temp;
                end() -= DEFAULT_SIZE;
                begin() -= DEFAULT_SIZE;
            } else if (((end() - 1) / DEFAULT_SIZE) < (_buffer().size() - 1)) {
                int last_valid_block_index = (end() - 1) / DEFAULT_SIZE;
                _buffer().push_front(_buffer()[(end() - 1) / DEFAULT_SIZE]);
                /*the pointer to last valid block push front to the io_buffer queue
                ,now this index increment,and the index after increment storage
                the same pointer with the first,we have to inplace it with the pointer
                in queue's back which was unused now*/
                last_valid_block_index++;
                _buffer()[last_valid_block_index] = _buffer().back();
                _buffer().pop_back();

            } else {
                _buffer().push_front(_buffer()[(end() - 1) / DEFAULT_SIZE]);
                _buffer().pop_back();
            }
            n -= DEFAULT_SIZE;
        }

    }

    char *first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
    char *last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];

    while (n > 0) {
        if (begin() == 0) {
            if (((end() - 1) / DEFAULT_SIZE) < (_buffer().size() - 1)) {
                char *temp = _buffer().back();
                _buffer().push_front(temp);
                _buffer().pop_back();
                begin() += DEFAULT_SIZE;
                end() += DEFAULT_SIZE;
            } else {
                char *temp = new char[DEFAULT_SIZE];
                _buffer().push_front(temp);
                begin() += DEFAULT_SIZE;
                end() += DEFAULT_SIZE;
            }
            first_block_ptr = _buffer()[(begin() - 1) / DEFAULT_SIZE];
            last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
        }
        if (begin() % DEFAULT_SIZE == 0)
            first_block_ptr = _buffer()[(begin() - 1) / DEFAULT_SIZE];
        if (end() % DEFAULT_SIZE == 0)
            last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
        begin()--;
        n--;
        first_block_ptr[begin() % DEFAULT_SIZE] =
                last_block_ptr[(end() - 1) % DEFAULT_SIZE];

        end()--;
    }
    check();
    __shrink_size();
}


void io_buffer::other_pop_back_to_front_n(io_buffer &other, size_t n) {
    check();
    other.check();

    if (begin() == (size_t) -1) {
        return;
    }

    if (n >= end() - begin()) {
        n = end() - begin();
    }

    if ((other.begin()) == (size_t) -1) {
        if (other._buffer().empty()) {
            if (((end() - 1) / DEFAULT_SIZE) < (_buffer().size() - 1)) {
                other._buffer().push_back(_buffer().back());
                _buffer().pop_back();
            } else if ((begin() / DEFAULT_SIZE) > 0) {
                other._buffer().push_back(_buffer().front());
                _buffer().pop_front();
                begin() -= DEFAULT_SIZE;
                end() -= DEFAULT_SIZE;
            } else {
                char *temp = new char[DEFAULT_SIZE];
                other._buffer().push_back(temp);
            }
        }
        other.begin() = end() % DEFAULT_SIZE;
        other.end() = other.begin();
    }

    if (((end() - other.begin()) % DEFAULT_SIZE == 0) && (n > 2 * DEFAULT_SIZE)) {
        int last = (end() - 1) % DEFAULT_SIZE;

        n -= ((last + 1) < DEFAULT_SIZE ? (last + 1) : 0);
        char *this_last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
        char *other_first_block_ptr = other._buffer()[other.begin() / DEFAULT_SIZE];
        while (last >= 0 && last < DEFAULT_SIZE - 1) {
            other.begin()--;
            other_first_block_ptr[other.begin() % DEFAULT_SIZE] =
                    this_last_block_ptr[(end() - 1) % DEFAULT_SIZE];
            last--;
            end()--;
        }
        while (n > DEFAULT_SIZE) {
            if (other.begin() != 0) {
                char *temp = other._buffer()[(other.begin() - 1) / DEFAULT_SIZE];
                other._buffer()[(other.begin() - 1) / DEFAULT_SIZE] = _buffer()[(end() - 1) / DEFAULT_SIZE];
                _buffer()[(end() - 1) / DEFAULT_SIZE] = temp;
                end() -= DEFAULT_SIZE;
                other.begin() -= DEFAULT_SIZE;
            } else if (((end() - 1) / DEFAULT_SIZE) < (_buffer().size() - 1)) {
                other._buffer().push_front(_buffer()[(end() - 1) / DEFAULT_SIZE]);
                _buffer()[(end() - 1) / DEFAULT_SIZE] = _buffer().back();
                _buffer().pop_back();

                end() -= DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;

            } else {
                other._buffer().push_front(_buffer()[(end() - 1) / DEFAULT_SIZE]);
                _buffer().pop_back();

                end() -= DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;
            }
            n -= DEFAULT_SIZE;
        }
    }


    char *this_last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
    char *other_first_block_ptr = other._buffer()[other.begin() / DEFAULT_SIZE];
    while (n > 0) {
        if (other.begin() == 0) {
            if (((end() - 1) / DEFAULT_SIZE) < (_buffer().size() - 1)) {
                char *temp = _buffer().back();
                other._buffer().push_front(temp);
                _buffer().pop_back();
                other.begin() += DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;
            } else if ((other._buffer().size() > 0) &&
                       (((other.end() - 1) / DEFAULT_SIZE) < (other._buffer().size() - 1))) {
                char *temp = other._buffer().back();
                other._buffer().push_front(temp);
                other._buffer().pop_back();

                other.begin() += DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;
            } else {
                char *temp = new char[DEFAULT_SIZE];
                other._buffer().push_front(temp);
                other.begin() += DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;
            }
            this_last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
            other_first_block_ptr = other._buffer()[other.begin() / DEFAULT_SIZE];
        }
        if ((other.begin() % DEFAULT_SIZE) == 0)
            other_first_block_ptr = other._buffer()[(other.begin() - 1) / DEFAULT_SIZE];
        if ((end() % DEFAULT_SIZE) == 0)
            this_last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
        other.begin()--;
        n--;
        other_first_block_ptr[other.begin() % DEFAULT_SIZE] =
                this_last_block_ptr[(end() - 1) % DEFAULT_SIZE];

        end()--;
    }
    check();
    other.check();
    __shrink_size();
    other.__shrink_size();
}


void io_buffer::self_pop_front_to_back_n(size_t n) {
    check();
    if ((begin() == (size_t) -1) || (n == 0) || (n >= (end() - begin()))) {
        return;
    }
    if (((end() - begin()) % DEFAULT_SIZE == 0) && (n > 2 * DEFAULT_SIZE)) {
        int first = begin() % DEFAULT_SIZE;

        char *first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
        char *last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];

        n -= (first % DEFAULT_SIZE == 0 ? 0 : DEFAULT_SIZE - first);
        while ((first % DEFAULT_SIZE) != 0) {

            last_block_ptr[end() % DEFAULT_SIZE] =
                    first_block_ptr[begin() % DEFAULT_SIZE];
            first++;
            end()++;
            begin()++;
        }
        while (n > DEFAULT_SIZE) {
            if (end() != (_buffer().size() * DEFAULT_SIZE)) {
                char *temp = _buffer()[end() / DEFAULT_SIZE];
                _buffer()[end() / DEFAULT_SIZE] = _buffer()[begin() / DEFAULT_SIZE];
                _buffer()[begin() / DEFAULT_SIZE] = temp;
                end() += DEFAULT_SIZE;
                begin() += DEFAULT_SIZE;
            } else if (begin() != 0) {
                _buffer().push_back(_buffer()[begin() / DEFAULT_SIZE]);
                _buffer()[begin() / DEFAULT_SIZE] = _buffer().front();
                _buffer().pop_front();

            } else {
                _buffer().push_back(_buffer()[begin() / DEFAULT_SIZE]);
                _buffer().pop_front();
            }
            n -= DEFAULT_SIZE;
        }

    }

    char *first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
    char *last_block_ptr = _buffer()[(end() - 1) / DEFAULT_SIZE];
    while (n > 0) {
        if (end() == (_buffer().size() * DEFAULT_SIZE)) {
            if (begin() >= DEFAULT_SIZE) {
                char *temp = _buffer().front();
                _buffer().push_back(temp);
                _buffer().pop_front();
                begin() -= DEFAULT_SIZE;
                end() -= DEFAULT_SIZE;
            } else {
                char *temp = new char[DEFAULT_SIZE];
                _buffer().push_back(temp);

            }
        }
        if (end() % DEFAULT_SIZE == 0)
            last_block_ptr = _buffer()[end() / DEFAULT_SIZE];
        if (begin() % DEFAULT_SIZE == 0)
            first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
        n--;
        last_block_ptr[end() % DEFAULT_SIZE] =
                first_block_ptr[begin() % DEFAULT_SIZE];
        end()++;
        begin()++;
    }
    check();
    __shrink_size();
}


void io_buffer::other_pop_front_to_back_n(io_buffer &other, size_t n) {
    check();
    other.check();
    if ((begin() == (size_t) -1) || (n == 0)) {
        return;
    }
    if (n >= end() - begin())
        n = end() - begin();

    if (other.begin() == (size_t) -1) {
        if (other._buffer().empty()) {
            if (((end() - 1) / DEFAULT_SIZE) < (_buffer().size() - 1)) {
                other._buffer().push_back(_buffer().back());
                _buffer().pop_back();
            } else if ((begin() / DEFAULT_SIZE) > 0) {
                other._buffer().push_back(_buffer().front());
                _buffer().pop_front();
                begin() -= DEFAULT_SIZE;
                end() -= DEFAULT_SIZE;
            } else {
                char *temp = new char[DEFAULT_SIZE];
                other._buffer().push_back(temp);
            }
        }
        other.begin() = begin() % DEFAULT_SIZE;
        other.end() = other.begin();
    }

    if (((begin() - other.end()) % DEFAULT_SIZE == 0) && (n > 2 * DEFAULT_SIZE)) {
        int first = begin() % DEFAULT_SIZE;

        n -= (first % DEFAULT_SIZE == 0 ? 0 : DEFAULT_SIZE - first);

        if ((first % DEFAULT_SIZE) != 0) {
            char *this_first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
            char *other_last_block_ptr = other._buffer()[other.end() / DEFAULT_SIZE];
            while ((first % DEFAULT_SIZE) != 0) {
                other_last_block_ptr[other.end() % DEFAULT_SIZE] =
                        this_first_block_ptr[begin() % DEFAULT_SIZE];
                first++;
                other.end()++;
                begin()++;
            }
        }
        while (n > DEFAULT_SIZE) {
            if (other.end() != (other._buffer().size() * DEFAULT_SIZE)) {
                char *temp = other._buffer()[other.end() / DEFAULT_SIZE];
                other._buffer()[other.end() / DEFAULT_SIZE] = _buffer()[begin() / DEFAULT_SIZE];
                _buffer()[begin() / DEFAULT_SIZE] = temp;
                begin() += DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;
            } else if (begin() != 0) {
                other._buffer().push_back(_buffer()[begin() / DEFAULT_SIZE]);
                _buffer()[begin() / DEFAULT_SIZE] = _buffer().front();
                _buffer().pop_front();

                end() -= DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;

            } else {
                other._buffer().push_back(_buffer()[begin() / DEFAULT_SIZE]);
                _buffer().pop_front();

                end() -= DEFAULT_SIZE;
                other.end() += DEFAULT_SIZE;
            }
            n -= DEFAULT_SIZE;
        }
    }


    char *this_first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
    char *other_last_block_ptr = (other.end() % DEFAULT_SIZE == 0) ? nullptr : other._buffer()[(other.end() - 1) /
                                                                                               DEFAULT_SIZE];

    while (n > 0) {
        if (other.end() == (other._buffer().size() * DEFAULT_SIZE)) {
            if (begin() >= DEFAULT_SIZE) {
                char *temp = _buffer().front();
                other._buffer().push_back(temp);
                _buffer().pop_front();

                begin() -= DEFAULT_SIZE;
                end() -= DEFAULT_SIZE;
            } else if ((other._buffer().size() > 0) && (other.begin() >= DEFAULT_SIZE)) {
                char *temp = other._buffer().front();
                other._buffer().push_back(temp);
                other._buffer().pop_front();

                other.begin() -= DEFAULT_SIZE;
                other.end() -= DEFAULT_SIZE;

            } else {
                char *temp = new char[DEFAULT_SIZE];
                other._buffer().push_back(temp);
            }
        }

        if (other.end() % DEFAULT_SIZE == 0) {
            other_last_block_ptr = other._buffer()[other.end() / DEFAULT_SIZE];
        }
        if (begin() % DEFAULT_SIZE == 0) {
            this_first_block_ptr = _buffer()[begin() / DEFAULT_SIZE];
        }
        n--;
        other_last_block_ptr[other.end() % DEFAULT_SIZE] =
                this_first_block_ptr[begin() % DEFAULT_SIZE];
        other.end()++;
        begin()++;
    }
    check();
    other.check();
    __shrink_size();
    other.__shrink_size();
}

