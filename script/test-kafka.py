from kafka import KafkaAdminClient, KafkaConsumer, KafkaProducer
from kafka.admin import NewTopic

def main():
    try:
        admin = KafkaAdminClient(bootstrap_servers='localhost:9093')
        topic = NewTopic(name='protobuf-events', num_partitions=1, replication_factor=1)
        admin.create_topics([topic])
    except Exception as e:
        print(e)

if __name__ == "__main__":
    main()