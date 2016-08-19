# Introduction

RamFuzz is a clang-based tool for generating testing code that exercises individual methods and functions of the code under test.  The generated code prepares a method's input parameters, invokes the method with them, and checks the result.

# How to build

RamFuzz source is intended to go under `clang/tools/extra` and build from there, as described in [this](http://clang.llvm.org/docs/LibASTMatchersTutorial.html#step-1-create-a-clangtool) clang tutorial (see "Step 1: Create a ClangTool").  Drop the top-level RamFuzz directory into `clang/tools/extra` and add it (using `add_subdirectory`) to `clang/tools/extra/CMakeLists.txt`.  After that, the standard llvm build procedure should produce a `bin/ramfuzz` executable.

There are also unit tests in the `test` directory.  To build them, drop `test` into `clang/unittests`, add it to `clang/unittests/CMakeLists.txt`, and build the target `RamFuzzTests`.  It should produce an executable `tools/clang/unittests/RamFuzz/RamFuzzTests`.
