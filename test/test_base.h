#ifndef PB_MESSAGING_TEST_TEST_BASE
#define PB_MESSAGING_TEST_TEST_BASE

#include <iostream>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <events.pb.h>

/* Print functions for debugging */
std::ostream& operator<< (std::ostream& out, events::simple_event const& event);

/* Test adapters */
void test_pipe_adapter();
void test_socket_adapter();
void test_kafka_adapter();

#define num_runs 10000000

/* Adapter test generic */
template<template<typename> class AdapterT, typename ...Ts>
void adapter_pub_func(Ts... args)
{
    AdapterT<events::simple_event> pub(args...);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        event.set_st(i);
        event.set_et(i + 1);
        event.set_payload(1.0f);
        pub.publish(event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

template<template<typename> class AdapterT, typename ...Ts>
void adapter_subs_func(Ts... args)
{
    std::vector<events::simple_event> buf;
    buf.reserve(num_runs);
    std::function<void(events::simple_event&)> handler =
        [&buf](events::simple_event &event)
        {
            buf.push_back(event);
        };

    AdapterT<events::simple_event> subs(args..., handler);

    while (buf.size() < num_runs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (size_t i = 0; i < num_runs; i++) {
        events::simple_event &event = buf[i];
        EXPECT_EQ(event.st(), i);
        EXPECT_EQ(event.et(), i + 1);
        EXPECT_EQ(event.payload(), 1.0f);
    }
}

#undef num_runs

#endif // PB_MESSAGING_TEST_TEST_BASE