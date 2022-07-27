#ifndef PB_MESSAGING_ADAPTERS_KAFKA
#define PB_MESSAGING_ADAPTERS_KAFKA

#include <memory>
#include <string>
#include <thread>

#include <pb_messaging/kafka/handler.h>
#include <pb_messaging/interface/daemon.h>
#include <pb_messaging/util/perf_monitor.h>

namespace pb_messaging {
namespace adapter {

template<typename T> class kafka_subscriber;

template<typename T>
class kafka_subscriber_daemon :
    public daemon
{
private:
    kafka_subscriber<T> *_consumer;
    std::shared_ptr<RdKafka::KafkaConsumer> kafka_cons;
    std::function<void(T&)> handler;

public:
    kafka_subscriber_daemon(kafka_subscriber<T> *_consumer, std::shared_ptr<RdKafka::KafkaConsumer> kafka_cons, std::function<void(T&)> &handler) :
        daemon(_consumer),
        _consumer(_consumer),
        kafka_cons(kafka_cons),
        handler(handler)
    {}
    ~kafka_subscriber_daemon(){}

    void operator()()
    {
        T t;

        std::function<void(RdKafka::Message *msg)> callback =
            [this, &t](RdKafka::Message* msg)
            {
                t.ParseFromArray(msg->payload(), msg->len());
                handler(t);
            };

        while (this->run()) {
            RdKafka::Message *msg = kafka_cons->consume(1000);
            kafka::handle_message(msg, callback);

#ifdef ENABLE_MONITORING
            _consumer->_cnt++;
#endif
        }

        /* Stop kafka consumer */
        kafka_cons->close();
    }
};

template<typename T>
class kafka_publisher :
    public util::perf_monitor_target
{
private:
    std::shared_ptr<RdKafka::Producer> kafka_prod;
    std::string topic;
    kafka::kafka_drcb drcb;
    char buf[1000];
public:
    kafka_publisher(std::string brokers, std::string topic) :
        topic(topic)
    {
        std::string err_msg;

        auto conf = std::shared_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        if (conf->set("bootstrap.servers", brokers, err_msg) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error(err_msg);
        }

        if (conf->set("dr_cb", &drcb, err_msg) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error(err_msg);
        }

        kafka_prod = std::shared_ptr<RdKafka::Producer>(RdKafka::Producer::create(conf.get(), err_msg));
        if (!kafka_prod) {
            throw std::runtime_error("Failed to create producer: " + err_msg);
        }
#ifdef ENABLE_MONITORING
        this->start_monitor("Kafka Publisher");
#endif
    }

    ~kafka_publisher()
    {
        /* Wait maximum 10 seconds to publish messages still in queue */
        kafka_prod->flush(1000 * 10);
    }

    void publish(T &t)
    {
        uint32_t data_size = t.ByteSizeLong();
        t.SerializeToArray(buf, data_size);

        retry:

        auto err_code = kafka_prod->produce(
            topic, 0, RdKafka::Producer::RK_MSG_COPY,
            buf, data_size, NULL, 0, 0, NULL, NULL
        );

        if (err_code != RdKafka::ERR_NO_ERROR) {

            if (err_code == RdKafka::ERR__QUEUE_FULL) {
                /* block for max 1000ms */
                kafka_prod->poll(1000);
                goto retry;
            } else {
                std::cerr << "% Failed to produce to topic " << topic << ": "
                    << RdKafka::err2str(err_code) << std::endl;
                return;
            }
        }

#ifdef ENABLE_MONITORING
        this->_cnt++;
#endif
        kafka_prod->poll(0);
    }
};

template<typename T>
class kafka_subscriber :
    public daemon_owner,
    public util::perf_monitor_target
{
private:
    std::shared_ptr<RdKafka::KafkaConsumer> kafka_cons;
public:
    kafka_subscriber(std::string brokers, std::string topic_str, std::string group_id, std::function<void(T&)> &handler) :
        daemon_owner()
    {
        std::string err_msg;

        auto conf = std::shared_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

        if (conf->set("bootstrap.servers", brokers, err_msg) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error(err_msg);
        }

        if (conf->set("client.id", "client", err_msg) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error(err_msg);
        }

        if (conf->set("group.id", group_id, err_msg) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error(err_msg);
        }

        if (conf->set("auto.offset.reset", "latest", err_msg) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error(err_msg);
        }

        kafka_cons = std::shared_ptr<RdKafka::KafkaConsumer>(RdKafka::KafkaConsumer::create(conf.get(), err_msg));
        if (!kafka_cons) {
            throw std::runtime_error("Failed to create kafka consumer: " + err_msg);
        }

        std::vector<std::string> topics {topic_str};
        RdKafka::ErrorCode err = kafka_cons->subscribe(topics);
        if (err) {
            throw std::runtime_error("Failed to subscribe to topic " + topic_str + ": " + RdKafka::err2str(err));
        }
#ifdef ENABLE_MONITORING
        this->start_monitor("Kafka Subscriber");
#endif
        this->start_daemon(std::make_shared<std::thread>(kafka_subscriber_daemon<T>(this, kafka_cons, handler)));
    }

    ~kafka_subscriber()
    {
        this->terminate_daemon();
    }
};

}
}

#endif // PB_MESSAGING_ADAPTERS_KAFKA