#include <gtest/gtest.h>

#include <test_base.h>

TEST(AdapterTest, PipeTest){ test_pipe_adapter(); }
TEST(AdapterTest, SocketTest) { test_socket_adapter(); }
TEST(AdapterTest, KafkaTest) { test_kafka_adapter(); }