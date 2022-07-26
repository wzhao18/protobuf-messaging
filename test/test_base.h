#ifndef PB_MESSAGING_TEST_TEST_BASE
#define PB_MESSAGING_TEST_TEST_BASE

#include <iostream>

#include <gtest/gtest.h>

#include <events.pb.h>

/* Print functions for debugging */
std::ostream& operator<< (std::ostream& out, events::simple_event const& event);

/* Test adapters */
void test_pipe_adapter();
void test_socket_adapter();
void test_kafka_adapter();

#endif // PB_MESSAGING_TEST_TEST_BASE