#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>

#include <events.pb.h>

#include <proto_print.h>
#include <pb_messaging/adapters/pipe.h>

#define num_runs 10000

void producer(std::string pipe_path)
{
    pb_messaging::adapter::pipe_producer<events::simple_event> pipe_producer(pipe_path);

    events::simple_event event;
    for (size_t i = 0; i < num_runs; i++) {
        event.set_st(i);
        event.set_et(i + 1);
        event.set_payload(1.0f);
        pipe_producer.produce(event);
    }
}

void consumer(std::string pipe_path)
{   
    pb_messaging::adapter::pipe_consumer<events::simple_event> pipe_consumer(pipe_path);

    int32_t count = 0;
    events::simple_event event;

    for (size_t i = 0; i < num_runs; i++) {
        pipe_consumer.consume(event);
        count++;
        // std::cout << event << std::endl;
    }

    assert(count == num_runs);
}

void test_pipe() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    auto pipe_path = std::filesystem::current_path() / "pipe";
    if (mkfifo(pipe_path.string().c_str(), 0666) < 0) {

    }

    std::thread producer_t(producer, pipe_path);
    std::thread consumer_t(consumer, pipe_path);

    producer_t.join();
    consumer_t.join();
    
    google::protobuf::ShutdownProtobufLibrary();
}