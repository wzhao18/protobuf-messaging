set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(Protobuf_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB PROTO_FILES
  "${Protobuf_IMPORT_DIRS}/*.proto"
)

set(TEST_FILES
    test.cpp
)

add_executable(pb_messaging_test ${TEST_FILES})
target_link_libraries(pb_messaging_test gtest_main protobuf::libprotobuf)

set(_protobuf_include_path -I ${Protobuf_IMPORT_DIRS})
protobuf_generate(TARGET pb_messaging_test LANGUAGE cpp PROTOS ${PROTO_FILES})

include(GoogleTest)
gtest_discover_tests(pb_messaging_test)