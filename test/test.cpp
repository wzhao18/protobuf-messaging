#include <gtest/gtest.h>

#include <test_pipe.h>
#include <test_socket.h>

TEST(AdapterTest, PipeTest) { test_pipe(); }
TEST(AdapterTest, SocketTest) { test_socket(); }