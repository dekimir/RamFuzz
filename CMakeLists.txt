set(LLVM_LINK_COMPONENTS support)
add_subdirectory(lib)
add_subdirectory(valgen)
add_clang_executable(ramfuzz main.cpp)
target_link_libraries(ramfuzz PRIVATE clangRamFuzz)

add_custom_target(RamFuzzEndToEndTests
  COMMAND ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test/test.py ${CMAKE_BINARY_DIR}
  DEPENDS ramfuzz clang valgen)

add_custom_target(RamFuzzUnitTests
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/unittests/RamFuzzTests
  DEPENDS RamFuzzTests)

add_custom_target(check-ramfuzz DEPENDS RamFuzzEndToEndTests RamFuzzUnitTests)

add_subdirectory(unittests)
