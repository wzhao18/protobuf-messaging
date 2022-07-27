#ifndef PB_MESSAGING_SOCKET
#define PB_MESSAGING_SOCKET

#include <iostream>
#include <memory>
#include <thread>

#include <pb_messaging/socket/msg_client.h>
#include <pb_messaging/socket/msg_server.h>
#include <pb_messaging/util/perf_monitor.h>

namespace pb_messaging {
namespace adapter {

template<typename T>
class socket_publisher :
    public util::perf_monitor_target
{
private:
    boost::asio::io_context io_context;
    tcp::endpoint endpoint;
    socket::message_server<T> sock_server;
public:
    socket_publisher(uint32_t port) :
        endpoint(tcp::v4(), port),
        sock_server(io_context, endpoint)
    {
#ifdef ENABLE_MONITORING
        this->start_monitor("Socket Publisher");
#endif
        std::thread sock_server_t([this](){ io_context.run(); });
        sock_server_t.detach();
    }

    ~socket_publisher()
    {
        
        /* Somehow this does not work, having to detach the thread from throwing SegFault */
        io_context.stop(); 
    }

    void publish(T &t)
    {
        sock_server.send(t);
#ifdef ENABLE_MONITORING
        this->_cnt++;
#endif
    }
};

template<typename T>
class socket_subscriber :
    public util::perf_monitor_target
{
private:
    boost::asio::io_context io_context;
    tcp::resolver resolver;
    tcp::resolver::results_type endpoints;

    std::function<void(T&)> wrapped_handler;
    socket::message_client<T> sock_client;
    std::thread sock_client_t;
public:
    socket_subscriber(uint32_t port, std::function<void(T&)> &handler) :
        resolver(io_context),
        endpoints(resolver.resolve("localhost", std::to_string(port))),
        wrapped_handler([this, &handler](T& t){ handler(t); this->_cnt++; }),
#ifdef ENABLE_MONITORING
        sock_client(io_context, endpoints, wrapped_handler),
#else
        sock_client(io_context, endpoints, handler),
#endif
        sock_client_t([this](){ io_context.run(); })
    {
#ifdef ENABLE_MONITORING
        this->start_monitor("Socket Subscriber");
#endif
    }

    ~socket_subscriber()
    {
        io_context.stop();
        sock_client_t.join();
    }
};

}
}

#endif // PB_MESSAGING_SOCKET