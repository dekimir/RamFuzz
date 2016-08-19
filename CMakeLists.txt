set(LLVM_LINK_COMPONENTS support)

add_clang_executable(ramfuzz
  RamFuzz.hpp
  RamFuzz.cpp
  main.cpp
  )
target_link_libraries(ramfuzz
  clangTooling
  clangBasic
  clangASTMatchers
  )
