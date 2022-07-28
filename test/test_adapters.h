#ifndef PB_MESSAGING_TEST_TEST_ADAPTERS
#define PB_MESSAGING_TEST_TEST_ADAPTERS

#include <iostream>
#include <string>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <filesystem>

#include <gtest/gtest.h>

#include <pb_messaging/adapters/socket.h>
#include <pb_messaging/adapters/pipe.h>
#include <pb_messaging/adapters/kafka.h>

#define REGISTER_ADAPTER_TEST(ADAPTER_TEST) \
    template<typename DataT> \
    void ADAPTER_TEST( \
        size_t num_iters, \
        std::function<void(DataT&, size_t)> data_fn, \
        std::function<void(DataT&)> subs_handler, \
        std::function<void(void)> callback \
    ) \

/* Adapter test generic */
template<template<typename> class AdapterT, typename DataT, typename ...Ts>
void adapter_pub_func(size_t num_iters, std::function<void(DataT&, size_t)> data_fn, Ts... args)
{
    AdapterT<DataT> pub(args...);

    DataT data;
    for (size_t i = 0; i < num_iters; i++) {
        data_fn(data, i);
        pub.publish(data);
    }

    // Wait for a while to ensure data in buffer have been published
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

template<template<typename> class AdapterT, typename DataT, typename ...Ts>
void adapter_subs_func(std::function<void(DataT&)> subs_handler, std::function<void(void)> callback, Ts... args)
{
    AdapterT<DataT> subs(args..., subs_handler);
    callback();
}

/* Test adapters */
REGISTER_ADAPTER_TEST(test_pipe_adapter)
{
    auto pipe_path = std::filesystem::current_path() / "pipe";
    std::filesystem::remove(pipe_path);
    if (mkfifo(pipe_path.string().c_str(), 0666) < 0) {
        FAIL() << "Cannot create pipe in " << std::filesystem::current_path().string();
    }

    std::thread producer_t(
        adapter_pub_func<pb_messaging::adapter::pipe_publisher, DataT, std::string>,
        num_iters, data_fn, pipe_path
    );
    std::thread consumer_t(
        adapter_subs_func<pb_messaging::adapter::pipe_subscriber, DataT, std::string>,
        subs_handler, callback, pipe_path
    );

    producer_t.join();
    consumer_t.join();
}

REGISTER_ADAPTER_TEST(test_socket_adapter)
{
    uint32_t port = 33350;

    std::thread producer_t(
        adapter_pub_func<pb_messaging::adapter::socket_publisher, DataT, uint32_t>,
        num_iters, data_fn, port
    );
    std::thread consumer_t(
        adapter_subs_func<pb_messaging::adapter::socket_subscriber, DataT, uint32_t>,
        subs_handler, callback, port
    );

    producer_t.join();
    consumer_t.join();
}

REGISTER_ADAPTER_TEST(test_kafka_adapter)
{
    std::string brokers = "localhost:9093";
    std::string topic = "protobuf-events";
    std::string group_id = "GROUP_001";

    std::thread producer_t(
        adapter_pub_func<pb_messaging::adapter::kafka_publisher, DataT, std::string, std::string>,
        num_iters, data_fn, brokers, topic
    );
    std::thread consumer_t(
        adapter_subs_func<pb_messaging::adapter::kafka_subscriber, DataT, std::string, std::string, std::string>,
        subs_handler, callback, brokers, topic, group_id
    );

    producer_t.join();
    consumer_t.join();
}

#endif // PB_MESSAGING_TEST_TEST_ADAPTERS