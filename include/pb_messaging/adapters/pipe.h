#ifndef PB_MESSAGING_PIPE
#define PB_MESSAGING_PIPE

#include <fstream>
#include <iostream>
#include <string>

#include <google/protobuf/util/delimited_message_util.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <pb_messaging/adapters/adapter.h>

namespace pb_messaging {
namespace adapter {

template<typename T> class pipe_producer;
template<typename T> class pipe_consumer;

template<typename T>
class pipe_producer_daemon :
    public producer_daemon<T>
{
private:
    std::ofstream &pipe_stream;

public:
    pipe_producer_daemon(pipe_producer<T> *_producer, std::ofstream &pipe_stream) :
        producer_daemon<T>(_producer),
        pipe_stream(pipe_stream)
    {}
    ~pipe_producer_daemon(){}

    void operator()()
    {
        T t;
        while (this->run()) {
            if (this->wait_dequeue_timed(t, std::chrono::milliseconds(5))) {
                if (!google::protobuf::util::SerializeDelimitedToOstream(t, &pipe_stream)) {
                    std::cerr << "Fail to serialize data into output stream" << std::endl;
                    return;
                }
            }
        }

        while (this->try_dequeue(t)) {
            if (!google::protobuf::util::SerializeDelimitedToOstream(t, &pipe_stream)) {
                std::cerr << "Fail to serialize data into output stream" << std::endl;
                return;
            }
        }
    }
};

template<typename T>
class pipe_consumer_daemon :
    public consumer_daemon<T>
{
private:
    google::protobuf::io::CodedInputStream &coded_in;
public:
    pipe_consumer_daemon(pipe_consumer<T> *_consumer, google::protobuf::io::CodedInputStream &coded_in) :
        consumer_daemon<T>(_consumer),
        coded_in(coded_in)
    {}
    ~pipe_consumer_daemon(){}

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

            /* Check run again, in case terminated but queue is full */
            while (this->run()) {
                if (this->wait_enqueue_timed(t, std::chrono::milliseconds(5))) {
                    break;
                }
            }
        }
    }
};

template<typename T>
class pipe_producer :
    public producer<T>
{
private:
    std::ofstream pipe_stream;

public:
    pipe_producer(std::string pipe_path, uint32_t buf_size = 4096) :
        producer<T>(buf_size),
        pipe_stream(pipe_path, std::ios_base::out |  std::ios_base::binary)
    {
        this->start_daemon(std::make_shared<std::thread>(pipe_producer_daemon<T>(this, pipe_stream)));
    }

    ~pipe_producer()
    {
        this->destroy();
    }
};

template<typename T>
class pipe_consumer :
    public consumer<T>
{
private:
    std::ifstream pipe_stream;
    google::protobuf::io::IstreamInputStream raw_in;
    google::protobuf::io::CodedInputStream coded_in;
public:
    pipe_consumer(std::string pipe_path, uint32_t buf_size = 4096) :
        consumer<T>(buf_size),
        pipe_stream(pipe_path, std::ios_base::in | std::ios_base::binary),
        raw_in(&pipe_stream),
        coded_in(&raw_in)
    {
        this->start_daemon(std::make_shared<std::thread>(pipe_consumer_daemon<T>(this, coded_in)));
    }

    ~pipe_consumer()
    {
        this->destroy();
    }
};

}
}

#endif // PB_MESSAGING_PIPE