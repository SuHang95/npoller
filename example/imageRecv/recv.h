#ifndef _recv_h__
#define _recv_h__

#include<io/tcp.h>
#include<io/task.h>
#include<thread>
#include<vector>
#include<string>
#include<condition_variable>
#include<mutex>
#include<atomic>
#include<queue>

namespace Recv {
    class recv;

    struct frame;

    extern logger debug;

    class recvtask : public task {
    public:
        recvtask(const std::shared_ptr<tcp> &_conn,
                 const logger &_log, recv *ret);

        virtual void notify(io_op &io);

        inline int status() const {
            return state.load();
        }

    protected:
        void func_load();

        inline frame getframe(io_buffer &, size_t size);

        std::vector<std::function<void(io_op &)>> state_func;
        std::shared_ptr<tcp> conn;
        recv *receive;

        friend class recv;

        logger log;
    };

    //convert network byte order to host byte order
    inline unsigned long long ntohll(unsigned long long val) {
        if (__BYTE_ORDER == __LITTLE_ENDIAN) {
            return (((unsigned long long) ntohl((int) ((val << 32) >> 32))) << 32) |
                   (unsigned int) ntohl((int) (val >> 32));
        } else if (__BYTE_ORDER == __BIG_ENDIAN) {
            return val;
        }
    }

    //convert host byte order to network byte order
    inline unsigned long long htonll(unsigned long long val) {
        if (__BYTE_ORDER == __LITTLE_ENDIAN) {
            return (((unsigned long long) htonl((int) ((val << 32) >> 32))) << 32) |
                   (unsigned int) htonl((int) (val >> 32));
        } else if (__BYTE_ORDER == __BIG_ENDIAN) {
            return val;
        }
    }


    template<typename T>
    class temp_ptr {
    public:
        inline T *get() const {
            return (src.get() + offset);
        }

        std::shared_ptr<T> src;
        int offset;
    };

    struct frame {
        int image_len;
        std::string timestamp;
        temp_ptr<char> image;
        double x;
        double y;
        double z;
    };


    class comp_frame_less {
    public:
        bool operator()(const frame &lhs, const frame &rhs) {
            size_t len = lhs.timestamp.size() > rhs.timestamp.size() ? rhs.timestamp.size() : lhs.timestamp.size();
            for (size_t i = 0; i < len; i++) {
                if (lhs.timestamp[i] < rhs.timestamp[i])
                    return true;
            }
            return false;
        }
    };

    class comp_frame_greater {
    public:
        bool operator()(const frame &lhs, const frame &rhs) {
            size_t len = lhs.timestamp.size() > rhs.timestamp.size() ? rhs.timestamp.size() : lhs.timestamp.size();
            for (size_t i = 0; i < len; i++) {
                if (lhs.timestamp[i] > rhs.timestamp[i])
                    return true;
            }
            return false;
        }
    };

    class recv {
    public:
        recv(unsigned short int port, logger _log = logger("netlog", logger::INFO), uint8_t _use = 0);

        frame getframe();

        void addframe(const frame &);

        ~recv();

    private:
        //accept the new connection and add make new task,add them in
        //taskcontainer,add the connection in this task
        void accept();

        event_processor proc;
        logger log;
        std::mutex task_mutex;
        std::mutex result_mutex;
        std::list<std::shared_ptr<recvtask>> task_list;

        //that makes the last frame to be the top
        std::priority_queue<frame, std::vector<frame>, comp_frame_less> result_big;
        std::priority_queue<frame, std::vector<frame>, comp_frame_greater> result_small;
        std::mutex frame_mutex;
        std::condition_variable preque_cond;
        std::atomic<int> size;

        int fd;
        unsigned short int port;
        //if 1,use result_big.
        //if 0,use result_small
        uint8_t use;
    };

    inline frame recvtask::getframe(io_buffer &buff, size_t size) {
        if (size == 0)
            return frame();

        char timestamp[24] = {0};
        frame ret;
        unsigned long long x;
        unsigned long long y;
        uint32_t imagelen;

        size_t image_size = size - 43;


        std::shared_ptr<char> mem(new char[size]);
        buff.get_n(0, mem.get(), size);


        if (static_cast<int64_t>(image_size) < 0) {
            log.warn("The len was wrong,some error occur!");
            return frame();
        }

        memcpy(&x, mem.get(), 8);
        memcpy(&y, mem.get() + 8, 8);
        memcpy(timestamp, mem.get() + 16, 23);
        memcpy(&imagelen, mem.get() + 39, 4);

        imagelen = ntohl(imagelen);

        if (imagelen != static_cast<uint32_t>(image_size)) {
            log.warn("Some error exist,image length in transfer"
                     "data=%d,image_size compute is %llu", imagelen, image_size);
        }

        x = ntohll(x);
        y = ntohll(y);

        for (size_t i = 0; i < sizeof(double); i++) {
            reinterpret_cast<char *>(&(ret.x))[i] = reinterpret_cast<char *>(&x)[i];
            reinterpret_cast<char *>(&(ret.y))[i] = reinterpret_cast<char *>(&y)[i];
        }
        ret.image.src = mem;
        ret.image.offset = 43;
        ret.image_len = static_cast<uint32_t>(image_size);

        ret.timestamp = std::string(timestamp, 23);

        return ret;
    }

}

#endif 
