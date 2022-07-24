#ifndef PB_MESSAGING_SOCKET
#define PB_MESSAGING_SOCKET

#include <iostream>
#include <string>
#include <cstdlib>
#include <list>
#include <memory>
#include <set>
#include <mutex>
#include <thread>
#include <csignal>

#include <boost/asio.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <google/protobuf/util/delimited_message_util.h>

#include <pb_messaging/adapters/adapter.h>

namespace pb_messaging {
namespace adapter {

using boost::asio::ip::tcp;

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

template<typename T>
class socket_client
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
    socket_client(boost::asio::io_context& io_context, const tcp::resolver::results_type& endpoints) :
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

template<typename T>
class subscriber
{
public:
    virtual ~subscriber() {}
    virtual bool deliver(T*, size_t) = 0;
};

template<typename T>
class subscriber_pool
{
    typedef std::shared_ptr<subscriber<T>> subscriber_ptr;
private:
    std::set<subscriber_ptr> subscribers;
    std::mutex set_mtx;
public:
    void join(subscriber_ptr sub)
    {
        std::unique_lock<std::mutex> lock(set_mtx);
        subscribers.insert(sub);
    }

    void leave(subscriber_ptr sub)
    {
        std::unique_lock<std::mutex> lock(set_mtx);
        subscribers.erase(sub);
    }

    bool notify(T* data, size_t t)
    {
        bool success = true;

        // Will block if there are currently no subscribers
        while (true) {
            set_mtx.lock();
            
            if (!subscribers.empty()) {
                break;
            }

            set_mtx.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (auto sub: subscribers) {
            if (!sub->deliver(data, t)) {
                success = false;
            }
        }

        set_mtx.unlock();
        return success;
    }
};

template<typename T>
class stream_subscriber :
    public subscriber<T>,
    public std::enable_shared_from_this<stream_subscriber<T>>
{
private:
    tcp::socket _socket;
    subscriber_pool<T>& pool;
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
                    std::thread t(&stream_subscriber::leave, this->shared_from_this());
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
    stream_subscriber(tcp::socket socket, subscriber_pool<T> &pool) :
        _socket(std::move(socket)),
        pool(pool),
        _ostream(&buf)
    {}

    void start()
    {
        join();
        async_write();
    }

    bool deliver(T* data, size_t size)
    {
        {
            std::unique_lock<std::mutex> lock(buf_mtx);
            for (size_t i = 0; i < size; i++) {
                if (!google::protobuf::util::SerializeDelimitedToOstream(data[i], &_ostream)) {
                    std::cerr << "Fail to serialize data into output stream" << std::endl;
                    return false;
                }
            }
        }

        return true;
    }
};

template<typename T>
class socket_server
{
private:
    boost::asio::ip::tcp::tcp::acceptor acceptor_;
    subscriber_pool<T> pool;

    void async_accept()
    {
        std::cout << "Listening for connection ..." << std::endl;
        acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::tcp::socket socket)
            {
                if (!ec) {
                    std::cout << "Received connection" << std::endl;
                    auto subscriber = std::make_shared<stream_subscriber<T>>(std::move(socket), pool);
                    std::thread t(&stream_subscriber<T>::start, subscriber);
                    t.detach();
                }

                async_accept();
            }
        );
    }

public:
    socket_server(boost::asio::io_context& io_context, const boost::asio::ip::tcp::tcp::endpoint& endpoint) :
        acceptor_(io_context, endpoint)
    {
        async_accept();
    }
    ~socket_server(){}

    bool emit_data(T *data, size_t size)
    {
        pool.notify(data, size);
        return true;
    }

    void close()
    {
        acceptor_.cancel();
        acceptor_.close();
    }
};

template<typename T> class socket_producer;
template<typename T> class socket_consumer;

template<typename T>
class socket_producer_daemon :
    public producer_daemon<T>
{
private:
    socket_server<T> &sock_server;
public:
    socket_producer_daemon(socket_producer<T> *_producer, socket_server<T> &sock_server) :
        producer_daemon<T>(_producer),
        sock_server(sock_server)
    {}
    ~socket_producer_daemon(){}

    void operator()()
    {
        T t;
        while (this->run()) {
            if (this->wait_dequeue_timed(t, std::chrono::milliseconds(5))) {
                sock_server.emit_data(&t, 1);
            }
        }

        while (this->try_dequeue(t)) {
            sock_server.emit_data(&t, 1);
        }
    }
};

template<typename T>
class socket_consumer_daemon :
    public consumer_daemon<T>
{
private:
    boost::asio::io_context &io_context;
    socket_client<T> &sock_client;
public:
    socket_consumer_daemon(socket_consumer<T> *_consumer, boost::asio::io_context &io_context, socket_client<T> &sock_client) :
        consumer_daemon<T>(_consumer),
        io_context(io_context),
        sock_client(sock_client)
    {}
    ~socket_consumer_daemon(){}

    void operator()()
    {
        sock_client.register_callback(
            [this](T &t) {
                while (this->run()) {
                    if (this->wait_enqueue_timed(t, std::chrono::milliseconds(5))) {
                        break;
                    }
                }
            }
        );
        io_context.run();
    }
};

template<typename T>
class socket_producer :
    public producer<T>
{
private:
    boost::asio::io_context io_context;
    tcp::endpoint endpoint;
    socket_server<T> sock_server;
public:
    socket_producer(uint32_t port) :
        producer<T>(),
        endpoint(tcp::v4(), port),
        sock_server(io_context, endpoint)
    {
        std::thread sock_server_t([this](){ io_context.run(); });
        sock_server_t.detach();
        this->start_daemon(std::make_shared<std::thread>(socket_producer_daemon<T>(this, sock_server)));
    }

    ~socket_producer()
    {
        this->destroy();
        io_context.stop(); /* Somehow this does not work, having to detach the thread from throwing SegFault */
    }
};

template<typename T>
class socket_consumer :
    public consumer<T>
{
private:
    boost::asio::io_context io_context;
    tcp::resolver resolver;
    tcp::resolver::results_type endpoints;
    socket_client<T> sock_client;
public:
    socket_consumer(uint32_t port) :
        consumer<T>(),
        resolver(io_context),
        endpoints(resolver.resolve("localhost", std::to_string(port))),
        sock_client(io_context, endpoints)
    {
        this->start_daemon(std::make_shared<std::thread>(socket_consumer_daemon<T>(this, io_context, sock_client)));
    }

    ~socket_consumer()
    {
        io_context.stop();
        this->destroy();
    }
};

}
}

#endif // PB_MESSAGING_SOCKET