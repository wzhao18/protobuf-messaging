#ifndef PB_MESSAGING_SUBSCRIBER_SUBSCRIBER
#define PB_MESSAGING_SUBSCRIBER_SUBSCRIBER

#include <memory>
#include <set>
#include <mutex>
#include <thread>

namespace pb_messaging {
namespace subscriber {

template<typename T>
class subscriber
{
public:
    virtual ~subscriber() {}
    virtual bool deliver(T&) = 0;
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

    bool publish(T& data)
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
            if (!sub->deliver(data)) {
                success = false;
            }
        }

        set_mtx.unlock();
        return success;
    }
};


}
}

#endif // PB_MESSAGING_SUBSCRIBER_SUBSCRIBER