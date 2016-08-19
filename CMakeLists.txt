set(LLVM_LINK_COMPONENTS support)
add_subdirectory(lib)
add_clang_executable(ramfuzz main.cpp)
target_link_libraries(ramfuzz clangRamFuzz)
