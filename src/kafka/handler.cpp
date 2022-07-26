#include <memory>
#include <functional>

#include <librdkafka/rdkafkacpp.h>

#include <pb_messaging/kafka/handler.h>

namespace pb_messaging {
namespace kafka {

void handle_message(RdKafka::Message *msg, std::function<void(RdKafka::Message *msg)>& callback)
{
    switch (msg->err()) {
        case RdKafka::ERR__TIMED_OUT:
            break;
        case RdKafka::ERR_NO_ERROR: {
            callback(msg);
            break;
        }
        case RdKafka::ERR__UNKNOWN_TOPIC:
        case RdKafka::ERR__UNKNOWN_PARTITION:
        default:
            std::cerr << "Consume failed: " << msg->errstr() << std::endl;
    }
    delete msg;
}

}
}