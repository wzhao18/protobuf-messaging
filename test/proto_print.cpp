#include <iostream>

#include <events.pb.h>
#include <proto_print.h>

std::ostream& operator<< (std::ostream& out, events::simple_event const& event)
{
    out << "simple_event: { ";
    out << "st: " << event.st() << " ";
    out << "et: " << event.et() << " ";
    out << "payload: " << event.payload() << " ";
    out << "}";
    return out;
}