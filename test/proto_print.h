#ifndef PB_MESSAGING_TEST_TEST_BASE
#define PB_MESSAGING_TEST_TEST_BASE

#include <iostream>

#include <events.pb.h>

/* Print functions for debugging */
std::ostream& operator<< (std::ostream& out, events::simple_event const& event);

#endif // PB_MESSAGING_TEST_TEST_BASE