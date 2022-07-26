#ifndef PB_MESSAGING_KAFKA_CALLBACK
#define PB_MESSAGING_KAFKA_CALLBACK

#include <functional>
#include <memory>
#include <iostream>

#include <librdkafka/rdkafkacpp.h>

namespace pb_messaging {
namespace kafka {

void handle_message(RdKafka::Message*, std::function<void(RdKafka::Message *msg)>&);

class kafka_drcb :
    public RdKafka::DeliveryReportCb
{
public:
    void dr_cb(RdKafka::Message &message) {
        if (message.err()) {
            std::cerr << "% Message delivery failed: " << message.errstr() << std::endl;
        }
    }
};

}
}

#endif // PB_MESSAGING_KAFKA_CALLBACK