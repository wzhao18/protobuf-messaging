#ifndef PB_MESSAGING_PIPE
#define PB_MESSAGING_PIPE

#include <fstream>
#include <iostream>
#include <string>

#include <google/protobuf/util/delimited_message_util.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <pb_messaging/interface/daemon.h>

namespace pb_messaging {
namespace adapter {

template<typename T> class pipe_subscriber;

template<typename T>
class pipe_subscriber_daemon :
    public daemon
{
private:
    pipe_subscriber<T> *_subscriber;
    std::function<void(T&)> handler;
    google::protobuf::io::CodedInputStream &coded_in;
public:
    pipe_subscriber_daemon(pipe_subscriber<T> *_subscriber, std::function<void(T&)> handler, google::protobuf::io::CodedInputStream &coded_in) :
        daemon(_subscriber),
        _subscriber(_subscriber),
        handler(handler),
        coded_in(coded_in)
    {}
    ~pipe_subscriber_daemon(){}

    void operator()()
    {
        T t;
        bool clean_eof;

        while (this->run()) {
            
            if (!google::protobuf::util::ParseDelimitedFromCodedStream(&t, &coded_in, &clean_eof)) {
                if (!clean_eof) {
                    std::cerr << "Fail to parse message from stream" << std::endl;
                }
                break;
            }

            handler(t);
#ifdef ENABLE_MONITORING
            _subscriber->_cnt++;
#endif
        }
    }
};

template<typename T>
class pipe_publisher :
    public util::perf_monitor_target
{
private:
    std::ofstream pipe_stream;

public:
    pipe_publisher(std::string pipe_path) :
        pipe_stream(pipe_path, std::ios_base::out | std::ios_base::binary)
    {
#ifdef ENABLE_MONITORING
        this->start_monitor("Pipe Publisher");
#endif
    }
    ~pipe_publisher(){};

    void publish(T &t)
    {
        if (!google::protobuf::util::SerializeDelimitedToOstream(t, &pipe_stream)) {
            std::cerr << "Fail to serialize data into output stream" << std::endl;
        }
#ifdef ENABLE_MONITORING
        this->_cnt++;
#endif
    }
};

template<typename T>
class pipe_subscriber :
    public daemon_owner,
    public util::perf_monitor_target
{
private:
    std::ifstream pipe_stream;
    google::protobuf::io::IstreamInputStream raw_in;
    google::protobuf::io::CodedInputStream coded_in;
public:
    pipe_subscriber(std::string pipe_path, std::function<void(T&)> &handler) :
        daemon_owner(),
        pipe_stream(pipe_path, std::ios_base::in | std::ios_base::binary),
        raw_in(&pipe_stream),
        coded_in(&raw_in)
    {
#ifdef ENABLE_MONITORING
        this->start_monitor("Pipe Subscriber");
#endif
        this->start_daemon(std::make_shared<std::thread>(pipe_subscriber_daemon<T>(this, handler, coded_in)));
    }

    ~pipe_subscriber()
    {
        terminate_daemon();
    }
};

}
}

#endif // PB_MESSAGING_PIPE