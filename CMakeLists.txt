cmake_minimum_required(VERSION 3.30)
project(rand_pool)

set(CMAKE_CXX_STANDARD 23)

add_executable(rand_pool_test rand_pool_test.cpp)
target_link_libraries(rand_pool_test PRIVATE gsl
    pthread
gtest gtest_main)
