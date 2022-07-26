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
private:
    std::shared_ptr<std::thread> monitor_t;
    
protected:
    uint64_t _cnt = 0;
    uint64_t _bytes = 0;
    void start_monitor();

public:
    volatile bool in_progress = true;
    virtual ~perf_monitor_target(){
        in_progress = false;
        if (monitor_t && monitor_t->joinable()) {
            monitor_t->join();
        }
    }

    uint64_t cnt()
    {
        return _cnt;
    }

    uint64_t bytes()
    {
        return _bytes;
    }
};

class perf_monitor
{
private:
    perf_monitor_target* target;
    uint64_t cnt;
    uint64_t bytes;

public:
    perf_monitor(perf_monitor_target* target) :
        target(target),
        cnt(0),
        bytes(0)
    {}

    void operator()(int32_t interval_seconds)
    {
        while (target->in_progress) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_seconds * 1000));
            uint64_t curr_cnt = target->cnt();
            uint64_t curr_bytes = target->bytes();

            int t_per_sec = -1;
            float mb_per_sec = -1;

            if (curr_cnt >= cnt)
                t_per_sec = (curr_cnt - cnt) / interval_seconds;

            if (curr_bytes >= bytes)
                mb_per_sec = static_cast<float>(curr_bytes - bytes) / static_cast<float>(interval_seconds) / 1024.0 / 1024.0;

            if (t_per_sec >= 0 && mb_per_sec >= 0) {
                std::stringstream result;
                result << "[Performance Monitor] Throughput: " << t_per_sec << " t/sec " << mb_per_sec << " MB/sec\n";
                std::cout << result.str();
            }

            cnt = curr_cnt;
            bytes = curr_bytes;
        }
    }
};

inline void perf_monitor_target::start_monitor()
{
    monitor_t = std::make_shared<std::thread>(perf_monitor(this), 1);
}

}
}

#endif // PB_MESSAGING_UTIL_PERF_MONITOR_H_