#include <cstdlib>

#include <gtest/gtest.h>

#include <events.pb.h>
#include <proto_print.h>
#include <test_adapters.h>

TEST(AdapterTest, CorrectnessTest)
{
    size_t num_iters = 10000;

    std::vector<events::simple_event> original;
    std::vector<events::simple_event> received;

    std::function<void(events::simple_event&, size_t)> data_fn = 
        [&original](events::simple_event &event, size_t i)
        {
            event.set_st(i);
            event.set_et(i + 1);
            event.set_payload(static_cast <float> (rand()) / static_cast <float> (RAND_MAX));
            original.push_back(event);
        };

    std::function<void(events::simple_event&)> subs_handler =
        [&received](events::simple_event& event)
        {
            received.push_back(event);
        };
    
    std::function<void(void)> callback = 
        [&received, &original, num_iters]()
        {
            while(received.size() != num_iters) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            for (size_t i = 0; i < num_iters; i++) {
                EXPECT_EQ(original[i].st(), received[i].st());
                EXPECT_EQ(original[i].et(), received[i].et());
                EXPECT_EQ(original[i].payload(), received[i].payload());
            }

            received.clear();
            original.clear();
        };


    test_pipe_adapter(num_iters, data_fn, subs_handler, callback);
    test_socket_adapter(num_iters, data_fn, subs_handler, callback);
    test_kafka_adapter(num_iters, data_fn, subs_handler, callback);
}

TEST(AdapterTest, PerformanceTest)
{
    size_t num_iters = 10000000;
    uint32_t count = 0;

    std::function<void(events::simple_event&, size_t)> data_fn = 
        [](events::simple_event &event, size_t i)
        {
            event.set_st(i);
            event.set_et(i + 1);
            event.set_payload(1.0f);
        };

    std::function<void(events::simple_event&)> subs_handler =
        [&count](events::simple_event& event)
        {
            count++;
        };
    
    std::function<void(void)> callback = 
        [&count, num_iters]()
        {
            while(count != num_iters) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            count = 0;
        };


    test_pipe_adapter(num_iters, data_fn, subs_handler, callback);
    test_socket_adapter(num_iters, data_fn, subs_handler, callback);
    test_kafka_adapter(num_iters, data_fn, subs_handler, callback);
}