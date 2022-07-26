#ifndef PB_MESSAGING_ADAPTER
#define PB_MESSAGING_ADAPTER

#include <fstream>
#include <iostream>
#include <string>
#include <memory>
#include <thread>

#include <readerwritercircularbuffer.h>
#include <google/protobuf/util/delimited_message_util.h>

#include <pb_messaging/util/perf_monitor.h>

namespace pb_messaging {
namespace adapter {

template<typename T> class adapter;
template<typename T> class producer;
template<typename T> class consumer;

template<typename T>
class adapter_daemon
{
private:
    virtual void operator()() = 0;

protected:
    adapter<T> *_adapter;

    bool run()
    {
        return _adapter->run;
    }

public:
    adapter_daemon(adapter<T> *_adapter) :
        _adapter(_adapter)
    {}
    ~adapter_daemon(){}
};

template<typename T>
class producer_daemon :
    public adapter_daemon<T>
{
protected:
    bool try_dequeue(T &t)
    {
        bool success = this->_adapter->_queue.try_dequeue(t);

#ifdef ENABLE_MONITORING
        if (success) {
            this->_adapter->_cnt++;
            this->_adapter->_bytes += t.ByteSizeLong();
        }
#endif
        return success;
    }

public:
    producer_daemon(producer<T> *_producer) :
        adapter_daemon<T>(_producer)
    {}
    ~producer_daemon(){}
};

template<typename T>
class consumer_daemon :
    public adapter_daemon<T>
{
protected:
    template<class Rep, class Period>
    bool wait_enqueue_timed(T &t, const std::chrono::duration<Rep, Period>& duration)
    {
        bool success = this->_adapter->_queue.wait_enqueue_timed(t, duration);

#ifdef ENABLE_MONITORING
        if (success) {
            this->_adapter->_cnt++;
            this->_adapter->_bytes += t.ByteSizeLong();
        }
#endif

        return success;
    }

public:
    consumer_daemon(adapter<T> *_consumer) :
        adapter_daemon<T>(_consumer)
    {}
    ~consumer_daemon(){}
};

template<typename T>
class adapter :
   public util::perf_monitor_target
{
friend class adapter_daemon<T>;
friend class producer_daemon<T>;
friend class consumer_daemon<T>;
private:
    std::shared_ptr<std::thread> daemon_t;
    bool enable_monitor;

protected:
    moodycamel::BlockingReaderWriterCircularBuffer<T> _queue;
    volatile bool run = true;

    void start_daemon(std::shared_ptr<std::thread> thread)
    {
        daemon_t = thread;
    }

    void destroy()
    {
        run = false;
        if (daemon_t->joinable()) {
            daemon_t->join();
        }
    }

public:
    adapter(uint32_t buf_size) :
        _queue(buf_size)
    {
#ifdef ENABLE_MONITORING
        this->start_monitor();
#endif
    }

    ~adapter()
    {
        destroy();
    }
};

template<typename T>
class producer :
    public adapter<T>
{
public:
    producer(uint32_t buf_size) :
        adapter<T>(buf_size)
    {}
    ~producer(){}

    void produce(T &t)
    {
        this->_queue.wait_enqueue(t);
    }
};

template<typename T>
class consumer :
    public adapter<T>
{
public:
    consumer(uint32_t buf_size) :
        adapter<T>(buf_size)
    {}
    ~consumer(){}

    void consume(T &t)
    {
        this->_queue.wait_dequeue(t);
    }
};

}
}

#endif // PB_MESSAGING_ADAPTER