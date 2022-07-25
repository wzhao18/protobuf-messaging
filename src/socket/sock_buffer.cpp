#include <memory>

#include <pb_messaging/socket/sock_buffer.h>

namespace pb_messaging {
namespace socket {

int64_t sock_buffer::size()
{
    return _size;
}

int64_t sock_buffer::volumn()
{
    return _volumn;
}

int64_t sock_buffer::remain()
{
    return _volumn - _size;
}

bool sock_buffer::consume(int64_t bytes_consumed)
{
    if (_size < bytes_consumed)
        return false;

    uint8_t* start = buf.get();
    memcpy(start, start + bytes_consumed, _size - bytes_consumed);
    _size -= bytes_consumed;
    return true;
}

void sock_buffer::write(int64_t bytes_written)
{
    _size += bytes_written;
}

uint8_t* sock_buffer::raw()
{
    return buf.get();
}

uint8_t* sock_buffer::raw_curr()
{
    return buf.get() + _size;
}

}
}