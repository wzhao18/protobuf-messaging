#include <iostream>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include <events.pb.h>

#include <proto_print.h>
#include <pb_messaging/adapters/socket.h>

#define num_runs 10000
#define port 33450

using boost::asio::ip::tcp;

std::atomic<bool> finished = false;

void socket_prod_func()
{
    pb_messaging::adapter::socket_producer<events::simple_event> socket_producer(port);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        event.set_st(i);
        event.set_et(i + 1);
        event.set_payload(1.0f);
        socket_producer.produce(event);
    }

    /* Keep the server running until consumer has consumed all events */
    while (!finished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void socket_cons_func()
{
    {
        pb_messaging::adapter::socket_consumer<events::simple_event> socket_consumer(port);

        events::simple_event event;
        for (size_t i = 0; i < num_runs; i++) {
            socket_consumer.consume(event);
            EXPECT_EQ(event.st(), i);
            EXPECT_EQ(event.et(), i + 1);
            EXPECT_EQ(event.payload(), 1.0f);
        }
    }

    finished = true;
}

void test_socket() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::thread producer_t(socket_prod_func);
    std::thread consumer_t(socket_cons_func);

    producer_t.join();
    consumer_t.join();

    google::protobuf::ShutdownProtobufLibrary();
}

#undef num_runs
#undef port