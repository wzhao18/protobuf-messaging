#ifndef PB_MESSAGING_SOCKET_MESSAGE_SERVER
#define PB_MESSAGING_SOCKET_MESSAGE_SERVER

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include <boost/asio.hpp>
#include <google/protobuf/util/delimited_message_util.h>

#include <pb_messaging/interface/subscriber.h>

using boost::asio::ip::tcp;

namespace pb_messaging {
namespace socket {

template<typename T>
class message_subscriber :
    public subscriber::subscriber<T>,
    public std::enable_shared_from_this<message_subscriber<T>>
{
private:
    tcp::socket _socket;
    subscriber::subscriber_pool<T>& pool;
    boost::asio::streambuf buf;
    std::ostream _ostream;
    std::mutex buf_mtx;

    void async_write()
    {
        while (buf.size() == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        buf_mtx.lock();
        boost::asio::async_write(_socket, buf,
            [this](boost::system::error_code ec, std::size_t len)
            {
                if (!ec) {
                    buf.consume(len);
                }

                buf_mtx.unlock();

                if (ec) {
                    std::thread t(&message_subscriber::leave, this->shared_from_this());
                    t.detach();
                    return;
                }

                async_write();
            }
        );
    }

    void join()
    {
        pool.join(this->shared_from_this());
    }

    void leave()
    {
        pool.leave(this->shared_from_this());
    }

public:
    message_subscriber(tcp::socket socket, subscriber::subscriber_pool<T> &pool) :
        _socket(std::move(socket)),
        pool(pool),
        _ostream(&buf)
    {}

    void start()
    {
        join();
        async_write();
    }

    bool deliver(T& data)
    {
        {
            std::unique_lock<std::mutex> lock(buf_mtx);
            if (!google::protobuf::util::SerializeDelimitedToOstream(data, &_ostream)) {
                std::cerr << "Fail to serialize data into output stream" << std::endl;
                return false;
            }
        }

        return true;
    }
};

template<typename T>
class message_server
{
private:
    boost::asio::ip::tcp::tcp::acceptor acceptor_;
    subscriber::subscriber_pool<T> pool;

    void async_accept()
    {
        std::cout << "Listening for connection ..." << std::endl;
        acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::tcp::socket socket)
            {
                if (!ec) {
                    std::cout << "Received connection" << std::endl;
                    auto subscriber = std::make_shared<message_subscriber<T>>(std::move(socket), pool);
                    std::thread t(&message_subscriber<T>::start, subscriber);
                    t.detach();
                }

                async_accept();
            }
        );
    }

public:
    message_server(boost::asio::io_context& io_context, const boost::asio::ip::tcp::tcp::endpoint& endpoint) :
        acceptor_(io_context, endpoint)
    {
        async_accept();
    }
    ~message_server(){}

    bool send(T &data)
    {
        pool.publish(data);
        return true;
    }
};

}
}

#endif // PB_MESSAGING_SOCKET_MESSAGE_SERVER