cmake_minimum_required(VERSION 3.8)
project(RamFuzz.ValGen)
set(CMAKE_CXX_STANDARD 11)
add_library(RamFuzz.ValgenLib valgen.hpp valgen.cpp exetree.hpp exetree.cpp)

add_executable(valgen main.cpp)
target_link_libraries(valgen RamFuzz.ValgenLib zmqpp zmq)

find_package(GTest)
if(GTEST_LIBRARY)
  set(gtest GTest::GTest)
  set(gtest_main GTest::Main)
else()
  add_subdirectory(googletest)
  set(gtest gtest)
  set(gtest_main gtest_main)
endif()

add_executable(utests utest_main.cpp valgen_test.cpp)
target_link_libraries(utests ${gtest} RamFuzz.ValgenLib zmqpp zmq)

enable_testing()
add_test(allunittests utests)
