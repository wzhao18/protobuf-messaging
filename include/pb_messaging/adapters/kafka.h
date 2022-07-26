#ifndef PB_MESSAGING_ADAPTERS_KAFKA
#define PB_MESSAGING_ADAPTERS_KAFKA

#include <memory>
#include <string>
#include <thread>

#include <pb_messaging/kafka/handler.h>
#include <pb_messaging/adapters/adapter.h>

namespace pb_messaging {
namespace adapter {

template<typename T> class kafka_producer;
template<typename T> class kafka_consumer;

template<typename T>
class kafka_producer_daemon :
    public producer_daemon<T>
{
private:
    std::shared_ptr<RdKafka::Producer> kafka_prod;
    std::string topic;
    char buf[1000];

    bool publish(T &t)
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
                return false;
            }
        }

        kafka_prod->poll(0);
        return true;
    }
public:
    kafka_producer_daemon(kafka_producer<T> *_producer, std::shared_ptr<RdKafka::Producer> kafka_prod, std::string topic) :
        producer_daemon<T>(_producer),
        kafka_prod(kafka_prod),
        topic(topic)
    {}
    ~kafka_producer_daemon(){}

    void operator()()
    {
        T t;
        while (this->run()) {
            if (this->wait_dequeue_timed(t, std::chrono::milliseconds(5))) {
                publish(t);
            }
        }

        while (this->try_dequeue(t)) {
            publish(t);
        }
    }
};

template<typename T>
class kafka_consumer_daemon :
    public consumer_daemon<T>
{
private:
    std::shared_ptr<RdKafka::KafkaConsumer> kafka_cons;

public:
    kafka_consumer_daemon(kafka_consumer<T> *_consumer, std::shared_ptr<RdKafka::KafkaConsumer> kafka_cons) :
        consumer_daemon<T>(_consumer),
        kafka_cons(kafka_cons)
    {}
    ~kafka_consumer_daemon(){}

    void operator()()
    {
        T t;

        std::function<void(RdKafka::Message *msg)> callback =
            [this, &t](RdKafka::Message* msg)
            {
                t.ParseFromArray(msg->payload(), msg->len());
                while (this->run()) {
                    if (this->wait_enqueue_timed(t, std::chrono::milliseconds(5))) {
                        break;
                    }
                }
            };

        while (this->run()) {
            RdKafka::Message *msg = kafka_cons->consume(1000);
            kafka::handle_message(msg, callback);
        }

        /* Stop kafka consumer */
        kafka_cons->close();
    }
};

template<typename T>
class kafka_producer :
    public producer<T>
{
private:
    std::shared_ptr<RdKafka::Producer> kafka_prod;
    kafka::kafka_drcb drcb;
public:
    kafka_producer(std::string brokers, std::string topic, uint32_t buf_size = 4096) :
        producer<T>(buf_size)
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

        this->start_daemon(std::make_shared<std::thread>(kafka_producer_daemon<T>(this, kafka_prod, topic)));
    }

    ~kafka_producer()
    {
        this->destroy();
        kafka_prod->flush(1000 * 10); /* Wait maximum 10 seconds to publish messages still in queue */
    }
};

template<typename T>
class kafka_consumer :
    public consumer<T>
{
private:
    std::shared_ptr<RdKafka::KafkaConsumer> kafka_cons;
public:
    kafka_consumer(std::string brokers, std::string topic_str, std::string group_id, uint32_t buf_size = 4096) :
        consumer<T>(buf_size)
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
            std::cerr << "Failed to subscribe to " << topics.size()
                    << " topics: " << RdKafka::err2str(err) << std::endl;
            exit(1);
        }

        this->start_daemon(std::make_shared<std::thread>(kafka_consumer_daemon<T>(this, kafka_cons)));
    }

    ~kafka_consumer()
    {
        this->destroy();
    }
};

}
}

#endif // PB_MESSAGING_ADAPTERS_KAFKA