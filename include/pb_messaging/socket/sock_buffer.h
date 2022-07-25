#ifndef PB_MESSAGING_SOCKET_SOCK_BUFFER
#define PB_MESSAGING_SOCKET_SOCK_BUFFER

#include <memory>

#include <boost/interprocess/mapped_region.hpp>

#define PAGE_SIZE boost::interprocess::mapped_region::get_page_size()

namespace pb_messaging {
namespace socket {

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
    ~sock_buffer(){}

    /* Get the current size of the buffer in bytes */
    int64_t size();

    /* Get the volumn of the buffer in bytes */
    int64_t volumn();

    /* Get the remaining bytes of the buffer */
    int64_t remain();

    /* Write to the buffer */
    void write(int64_t);

    /* Consume from the buffer and free up space */
    bool consume(int64_t);

    /* get the raw pointer to the begin of buffer */
    uint8_t* raw();

    /* get the raw pointer to the current writable region of buffer */
    uint8_t* raw_curr();
};

}
}

#undef PAGE_SIZE

#endif // PB_MESSAGING_SOCKET_SOCK_BUFFER