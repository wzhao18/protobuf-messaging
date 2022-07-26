set(SRC_FILES
    socket/sock_buffer.cpp
    kafka/handler.cpp
)

add_library(pb_messaging STATIC ${SRC_FILES})
target_compile_options(pb_messaging PRIVATE -O3 -Wall -Wextra -pedantic -Werror)

