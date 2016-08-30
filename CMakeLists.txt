set(LLVM_LINK_COMPONENTS support)
add_subdirectory(lib)
add_clang_executable(ramfuzz main.cpp)
target_link_libraries(ramfuzz clangRamFuzz)

add_custom_target(ramfuzz-test
  COMMAND ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test/test.py ${CMAKE_BINARY_DIR}
  DEPENDS ramfuzz)
