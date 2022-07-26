#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <pb_messaging/adapters/kafka.h>

#include <events.pb.h>
#include <test_base.h>

#define num_runs 100000
#define buf_size 4096

void kafka_prod_func(std::string brokers, std::string topic)
{
    pb_messaging::adapter::kafka_producer<events::simple_event> kafka_producer(brokers, topic, buf_size);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        event.set_st(i);
        event.set_et(i + 1);
        event.set_payload(1.0f);
        kafka_producer.produce(event);
    }
}

void kafka_cons_func(std::string brokers, std::string topic, std::string group_id)
{   
    pb_messaging::adapter::kafka_consumer<events::simple_event> kafka_consumer(brokers, topic, group_id, buf_size);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        kafka_consumer.consume(event);
        EXPECT_EQ(event.st(), i);
        EXPECT_EQ(event.et(), i + 1);
        EXPECT_EQ(event.payload(), 1.0f);
    }
}

void test_kafka_adapter() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::string brokers = "localhost:9093";
    std::string topic = "protobuf-events";
    std::string group_id = "GROUP_001";

    std::thread producer_t(kafka_prod_func, brokers, topic);
    std::thread consumer_t(kafka_cons_func, brokers, topic, group_id);

    producer_t.join();
    consumer_t.join();
    
    google::protobuf::ShutdownProtobufLibrary();
}

#undef num_runs
#undef buf_size