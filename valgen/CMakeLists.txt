cmake_minimum_required(VERSION 3.8)
project(RamFuzz.ValGen)

find_package(Torch REQUIRED)

set(CMAKE_CXX_STANDARD 11)
add_library(RamFuzz.ValgenLib
  valgen.hpp valgen.cpp exetree.hpp exetree.cpp dataset.hpp dataset.cpp)
target_link_libraries(RamFuzz.ValgenLib ${TORCH_LIBRARIES} zmqpp zmq)

add_executable(valgen main.cpp)
target_link_libraries(valgen RamFuzz.ValgenLib)

add_executable(driver driver.cpp)
target_link_libraries(driver zmqpp zmq)

find_package(GTest)
if(GTEST_LIBRARY AND GTEST_INCLUDE_DIR AND GTEST_MAIN_LIBRARY)
  set(gtest GTest::GTest)
  set(gtest_main GTest::Main)
else()
  add_subdirectory(googletest)
  set(gtest gtest)
  set(gtest_main gtest_main)
endif()

add_executable(utests utest_main.cpp
  valgen_test.cpp exetree_test.cpp dataset_test.cpp)
target_link_libraries(utests ${gtest} RamFuzz.ValgenLib)

enable_testing()
add_test(allunittests utests)
