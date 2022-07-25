#ifndef PB_MESSAGING_SOCKET_SOCK_BUFFER
#define PB_MESSAGING_SOCKET_SOCK_BUFFER

#include <memory>

#include <boost/interprocess/mapped_region.hpp>

namespace pb_messaging {
namespace socket {

#define PAGE_SIZE boost::interprocess::mapped_region::get_page_size()

class sock_buffer
{
private:
    const int64_t _volumn;
    std::shared_ptr<uint8_t[]> buf;
    int64_t _size;
public:
    sock_buffer(int64_t volumn) :
        _volumn(((volumn + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE),
        buf((uint8_t*) aligned_alloc(PAGE_SIZE, _volumn)),
        _size(0)
    {}

    int64_t size()
    {
        return _size;
    }

    int64_t volumn()
    {
        return _volumn;
    }

    int64_t remain()
    {
        return _volumn - _size;
    }

    bool consume(int64_t bytes_consumed)
    {
        if (_size < bytes_consumed)
            return false;

        uint8_t* start = buf.get();
        memcpy(start, start + bytes_consumed, _size - bytes_consumed);
        _size -= bytes_consumed;
        return true;
    }

    void write(int64_t bytes_written)
    {
        _size += bytes_written;
    }

    uint8_t* raw()
    {
        return buf.get();
    }

    uint8_t* raw_curr()
    {
        return buf.get() + _size;
    }
};

#undef PAGE_SIZE

}
}

#endif // PB_MESSAGING_SOCKET_SOCK_BUFFER