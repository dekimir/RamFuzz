add_clang_library(clangRamFuzz ${ENABLE_SHARED} ${ENABLE_STATIC}
  Inheritance.hpp
  Inheritance.cpp
  RamFuzz.hpp
  RamFuzz.cpp
  DEPENDS clang-headers
  LINK_LIBS clangTooling clangBasic clangASTMatchers
  LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} Core Support
  )

# So unit tests can #include <ramfuzz/lib/...>
target_include_directories(clangRamFuzz
  PUBLIC $<BUILD_INTERFACE:${CLANG_SOURCE_DIR}/tools/extra>)
