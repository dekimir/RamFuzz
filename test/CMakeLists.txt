set(LLVM_LINK_COMPONENTS Support)
add_clang_unittest(RamFuzzTests FooTest.cpp)
target_link_libraries(RamFuzzTests clangRamFuzz)
