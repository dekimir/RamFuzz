add_clang_library(clangRamFuzz ${ENABLE_SHARED} ${ENABLE_STATIC}
  RamFuzz.hpp
  RamFuzz.cpp
  DEPENDS clang-headers
  LINK_LIBS clangTooling clangBasic clangASTMatchers
  LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} Core Support
  )
