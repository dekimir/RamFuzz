cmake_minimum_required(VERSION 3.8)
project(RamFuzz.ValGen)

find_package(Torch REQUIRED)

set(CMAKE_CXX_STANDARD 11)
add_library(RamFuzz.ValgenLib
  dataset.cpp
  dataset.hpp
  exetree.cpp
  exetree.hpp
  nnet.cpp
  nnet.hpp
  status.hpp
  valgen.cpp
  valgen.hpp
  )
target_link_libraries(RamFuzz.ValgenLib ${TORCH_LIBRARIES} zmqpp zmq)

add_executable(valgen main.cpp)
target_link_libraries(valgen RamFuzz.ValgenLib)

add_executable(driver driver.cpp status.hpp)
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

add_custom_target(SaveAModel ALL
  ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/save-test-model.py)

add_executable(utests
  dataset_test.cpp
  exetree_test.cpp
  nnet_test.cpp
  utest_main.cpp
  util.cpp
  util.hpp
  valgen_test.cpp
  )
target_link_libraries(utests ${gtest} RamFuzz.ValgenLib)

enable_testing()
add_test(allunittests utests)
