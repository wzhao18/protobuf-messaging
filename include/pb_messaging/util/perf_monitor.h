#ifndef PB_MESSAGING_UTIL_PERF_MONITOR_H_
#define PB_MESSAGING_UTIL_PERF_MONITOR_H_

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream> 

namespace pb_messaging {
namespace util {

class perf_monitor_target
{
friend class perf_monitor;
private:
    std::shared_ptr<std::thread> monitor_t;
    volatile bool in_progress = true;

protected:
    void start_monitor(std::string);

public:
    perf_monitor_target(){}
    virtual ~perf_monitor_target()
    {
        in_progress = false;
        if (monitor_t && monitor_t->joinable()) {
            monitor_t->join();
        }
    }

    uint64_t _cnt = 0;

    uint64_t cnt()
    {
        return _cnt;
    }
};

class perf_monitor
{
private:
    perf_monitor_target* target;
    std::string name;
    uint64_t cnt;

public:
    perf_monitor(perf_monitor_target* target, std::string name) :
        target(target),
        name(name),
        cnt(0)
    {}

    void operator()(int32_t interval_seconds)
    {
        while (target->in_progress) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_seconds * 1000));
            uint64_t curr_cnt = target->cnt();

            int t_per_sec = -1;

            if (curr_cnt >= cnt)
                t_per_sec = (curr_cnt - cnt) / interval_seconds;

            if (t_per_sec >= 0) {
                std::stringstream result;
                result << "[Performance Monitor - " << name << "] Throughput: " << t_per_sec << " t/sec\n";
                std::cout << result.str();
            }

            cnt = curr_cnt;
        }
    }
};

inline void perf_monitor_target::start_monitor(std::string name)
{
    monitor_t = std::make_shared<std::thread>(perf_monitor(this, name), 1);
}

}
}

#endif // PB_MESSAGING_UTIL_PERF_MONITOR_H_