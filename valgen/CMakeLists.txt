add_clang_executable(valgen main.cpp valgen.hpp valgen.cpp)
target_link_libraries(valgen PUBLIC zmqpp zmq)
