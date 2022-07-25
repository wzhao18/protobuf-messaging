#ifndef PB_MESSAGING_SOCKET_MESSAGE_CLIENT
#define PB_MESSAGING_SOCKET_MESSAGE_CLIENT

#include <iostream>
#include <memory>
#include <set>
#include <thread>

#include <boost/asio.hpp>
#include <google/protobuf/util/delimited_message_util.h>

#include <pb_messaging/socket/sock_buffer.h>

using boost::asio::ip::tcp;

namespace pb_messaging {
namespace socket {

template<typename T>
class message_client
{
    enum { buf_size = 65536 };
private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    sock_buffer buf;
    std::function<void(T&)> callback = [](T&){};

    void async_connect(const tcp::resolver::results_type& endpoints)
    {
        boost::asio::async_connect(socket_, endpoints,
            [this, endpoints](boost::system::error_code ec, tcp::endpoint)
            {
                if (!ec) {
                    async_read();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    async_connect(endpoints);
                }
            }
        );
    }

    void async_read()
    {
        boost::asio::async_read(socket_, boost::asio::buffer(buf.raw_curr(), buf.remain()), boost::asio::transfer_at_least(1),
            [this](boost::system::error_code ec, std::size_t len)
            {
                if (!ec) {
                    buf.write(len);

                    google::protobuf::io::CodedInputStream coded_in(buf.raw(), buf.size());
                    int64_t bytes_consumed = coded_in.CurrentPosition();
                    
                    T data;
                    bool clean_eof;

                    while (true) {
                        if (!google::protobuf::util::ParseDelimitedFromCodedStream(&data, &coded_in, &clean_eof)) {
                            break;
                        } else {
                            callback(data);
                            bytes_consumed = coded_in.CurrentPosition();
                        }
                    }
                    buf.consume(bytes_consumed);

                    async_read();
                }
                else {
                    std::cerr << "Fail to read from socket: " << ec.message() << std::endl;
                    close();
                }
            }
        );
    }

    void close()
    {
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }

public:
    message_client(boost::asio::io_context& io_context, const tcp::resolver::results_type& endpoints) :
        io_context_(io_context),
        socket_(io_context),
        buf(buf_size)
    {
        async_connect(endpoints);
    }

    void register_callback(std::function<void(T&)> _callback)
    {
        callback = _callback;
    }
};

}
}

#endif // PB_MESSAGING_SOCKET_MESSAGE_CLIENT