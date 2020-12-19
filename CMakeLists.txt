cmake_minimum_required(VERSION 3.19)
project(scq)

set(CMAKE_CXX_STANDARD 20)

add_library(scqueue INTERFACE)
target_include_directories(scqueue INTERFACE include/)

find_package(Threads REQUIRED)

add_executable(test_scq test/test_scq.cpp)
target_include_directories(test_scq PRIVATE include/)
target_link_libraries(test_scq PRIVATE Threads::Threads)

add_executable(test_simple test/test_simple.cpp)
target_include_directories(test_simple PRIVATE include/)

add_executable(test_simple_scq2 test/test_simple_scq2.cpp)
target_include_directories(test_simple_scq2 PRIVATE include/)
