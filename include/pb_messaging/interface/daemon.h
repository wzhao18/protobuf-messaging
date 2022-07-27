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

class daemon;

class daemon_owner
{
friend class daemon;
private:
    std::shared_ptr<std::thread> _daemon;
    volatile bool run = true;

protected:
    void start_daemon(std::shared_ptr<std::thread> __daemon)
    {
        _daemon = __daemon;
    }

    void terminate_daemon()
    {
        run = false;
        if (_daemon->joinable()) {
            _daemon->join();
        }
    }

public:
    daemon_owner(){}
    ~daemon_owner()
    {
        terminate_daemon();
    }
};

class daemon
{
private:
    virtual void operator()() = 0;

protected:
    daemon_owner *owner;

    bool run()
    {
        return owner->run;
    }

public:
    daemon(daemon_owner *owner) :
        owner(owner)
    {}
    ~daemon(){}
};

}
}

#endif // PB_MESSAGING_ADAPTER