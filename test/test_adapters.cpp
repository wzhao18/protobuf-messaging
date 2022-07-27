#include <iostream>
#include <string>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <filesystem>

#include <pb_messaging/adapters/socket.h>
#include <pb_messaging/adapters/pipe.h>
#include <pb_messaging/adapters/kafka.h>

#include <events.pb.h>
#include <test_base.h>


void test_socket_adapter() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    uint32_t port = 33350;

    std::thread producer_t(adapter_pub_func<pb_messaging::adapter::socket_publisher, uint32_t>, port);
    std::thread consumer_t(adapter_subs_func<pb_messaging::adapter::socket_subscriber, uint32_t>, port);

    producer_t.join();
    consumer_t.join();

    google::protobuf::ShutdownProtobufLibrary();
}

void test_pipe_adapter() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    auto pipe_path = std::filesystem::current_path() / "pipe";
    std::filesystem::remove(pipe_path);
    if (mkfifo(pipe_path.string().c_str(), 0666) < 0) {
        FAIL() << "Cannot create pipe in " << std::filesystem::current_path().string();
    }

    std::thread producer_t(adapter_pub_func<pb_messaging::adapter::pipe_publisher, std::string>, pipe_path);
    std::thread consumer_t(adapter_subs_func<pb_messaging::adapter::pipe_subscriber, std::string>, pipe_path);

    producer_t.join();
    consumer_t.join();

    google::protobuf::ShutdownProtobufLibrary();
}

void test_kafka_adapter() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::string brokers = "localhost:9093";
    std::string topic = "protobuf-events";
    std::string group_id = "GROUP_001";

    std::thread producer_t(adapter_pub_func<pb_messaging::adapter::kafka_publisher, std::string, std::string>, brokers, topic);
    std::thread consumer_t(adapter_subs_func<pb_messaging::adapter::kafka_subscriber, std::string, std::string, std::string>, brokers, topic, group_id);

    producer_t.join();
    consumer_t.join();
    
    google::protobuf::ShutdownProtobufLibrary();
}