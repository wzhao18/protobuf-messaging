#ifndef PB_MESSAGING_TEST_PROTO_PRINT
#define PB_MESSAGING_TEST_PROTO_PRINT

#include <iostream>

#include <events.pb.h>

std::ostream& operator<< (std::ostream& out, events::simple_event const& event)
{
    out << "simple_event: { ";
    out << "st: " << event.st() << " ";
    out << "et: " << event.et() << " ";
    out << "payload: " << event.payload() << " ";
    out << "}";
    return out;
}

#endif // PB_MESSAGING_TEST_PROTO_PRINT