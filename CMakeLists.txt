cmake_minimum_required(VERSION 3.16.3)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

project(protobuf-messaging)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(RdKafka CONFIG REQUIRED)

if(protobuf_VERBOSE)
  message(STATUS "Using Protocol Buffers ${protobuf_VERSION}")
endif()

add_subdirectory(third_party/googletest)

include_directories(include third_party/readerwriterqueue ~/.local/include)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)
