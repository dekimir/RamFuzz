Value generator for RamFuzz tests.  A standalone executable that
RamFuzz-generated code messages to get random values when necessary.

Build separately from RamFuzz, in valgen's own build directory.  Use cmake and
ctest.  According to LibTorch
[instructions](https://pytorch.org/cppdocs/installing.html), must pass
`-DCMAKE_PREFIX_PATH=/absolute/path/to/libtorch` to cmake.

Prerequisites:
- zmqpp >=4.2.0 (eg, `brew install zmqpp` or `apt install libzmqpp-dev`)
- googletest
  + if googletest is installed, set GTEST_ROOT to install prefix (parent of
    `include/gtest/gtest.h`), as described
    [here](https://cmake.org/cmake/help/latest/module/FindGTest.html)
  + otherwise, clone the [googletest repo](https://github.com/google/googletest)
    in this directory
- LibTorch (eg, from https://pytorch.org/get-started/locally/)
  + building valgen with a modern gcc (>= 5.1) is incompatible with LibTorch
    binaries downloaded from pytorch.org because of
    [this issue](https://discuss.pytorch.org/t/issues-linking-with-libtorch-c-11-abi/29510);
    workaround:
    [build LibTorch from scratch](https://github.com/pytorch/pytorch#from-source)
