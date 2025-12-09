cmake_minimum_required(VERSION 3.10)
project(test VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(opentelemetry-cpp CONFIG REQUIRED)

add_executable(test main.cpp)

target_link_libraries(test PRIVATE
    opentelemetry-cpp::otlp_http_exporter
    opentelemetry-cpp::ostream_span_exporter
)
