#ifndef PB_MESSAGING_SOCKET
#define PB_MESSAGING_SOCKET

#include <iostream>
#include <memory>
#include <thread>

#include <pb_messaging/adapters/adapter.h>
#include <pb_messaging/socket/msg_client.h>
#include <pb_messaging/socket/msg_server.h>

namespace pb_messaging {
namespace adapter {

template<typename T> class socket_producer;
template<typename T> class socket_consumer;

template<typename T>
class socket_producer_daemon :
    public producer_daemon<T>
{
private:
    socket::message_server<T> &sock_server;
public:
    socket_producer_daemon(socket_producer<T> *_producer, socket::message_server<T> &sock_server) :
        producer_daemon<T>(_producer),
        sock_server(sock_server)
    {}
    ~socket_producer_daemon(){}

    void operator()()
    {
        T t;
        while (this->run()) {
            if (this->wait_dequeue_timed(t, std::chrono::milliseconds(5))) {
                sock_server.send(t);
            }
        }

        while (this->try_dequeue(t)) {
            sock_server.send(t);
        }
    }
};

template<typename T>
class socket_consumer_daemon :
    public consumer_daemon<T>
{
private:
    boost::asio::io_context &io_context;
    socket::message_client<T> &sock_client;
public:
    socket_consumer_daemon(socket_consumer<T> *_consumer, boost::asio::io_context &io_context, socket::message_client<T> &sock_client) :
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
    socket::message_server<T> sock_server;
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
    socket::message_client<T> sock_client;
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