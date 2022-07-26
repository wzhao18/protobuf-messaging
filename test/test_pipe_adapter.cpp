#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#include <thread>

#include <pb_messaging/adapters/pipe.h>

#include <events.pb.h>
#include <test_base.h>

#define num_runs 100000
#define buf_size 4096

void pipe_prod_func(std::string pipe_path)
{
    pb_messaging::adapter::pipe_producer<events::simple_event> pipe_producer(pipe_path, buf_size);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        event.set_st(i);
        event.set_et(i + 1);
        event.set_payload(1.0f);
        pipe_producer.produce(event);
    }
}

void pipe_cons_func(std::string pipe_path)
{   
    pb_messaging::adapter::pipe_consumer<events::simple_event> pipe_consumer(pipe_path, buf_size);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        pipe_consumer.consume(event);
        EXPECT_EQ(event.st(), i);
        EXPECT_EQ(event.et(), i + 1);
        EXPECT_EQ(event.payload(), 1.0f);
    }
}

void test_pipe_adapter() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    auto pipe_path = std::filesystem::current_path() / "pipe";
    std::filesystem::remove(pipe_path);
    if (mkfifo(pipe_path.string().c_str(), 0666) < 0) {
        FAIL() << "Cannot create pipe in " << std::filesystem::current_path().string();
    }

    std::thread producer_t(pipe_prod_func, pipe_path);
    std::thread consumer_t(pipe_cons_func, pipe_path);

    producer_t.join();
    consumer_t.join();
    
    google::protobuf::ShutdownProtobufLibrary();
}

#undef num_runs
#undef buf_size