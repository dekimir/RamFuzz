# RamFuzz

RamFuzz is a fuzzer for individual method parameters in unit tests.  A unit test can use RamFuzz to generate random parameter values for methods under test.  The values are logged, and the log can be replayed to repeat the exact same test scenario.  But RamFuzz also allows mutation of the replay, where some parts of the log are replayed while others are replaced by newly generated values.  The new run is also logged, yielding a mutated test scenario and allowing the classic fuzzing evolution process of progressively mutating the input until a bug is triggered.

This allows a combination of two testing techniques so powerful they have been [called](https://ep2015.europython.eu/conference/talks/testing-with-two-failure-seeking-missiles-fuzzing-and-property-based-testing) "failure seeking missiles": property-based testing and fuzz testing.  Property-based testing verifies code invariants over a wide range of randomly generated parameter values -- essentially unit testing with endless variations of every test case.  Fuzzing is a way of mutating a program's input until it fails.  Both have been used to discover many hard-to-find bugs but have not, so far, been combined in a general tool.  As discussed on [this blog](http://danluu.com/testing/), combining them should make property-based testing find bugs faster and more directly.

RamFuzz also includes a tool that can generate random objects of any class.  It works by reading the class source code and generating from it test code that creates an instance via a randomly chosen constructor, then proceeds to invoke a random sequence of instance methods with randomly generated arguments.  The resulting object can then be used in property testing of methods that take it as a parameter.  The generated code can also be manually adapted to itself become property tests.

RamFuzz currently supports C++ (with some limitations that we're working to remove -- please see "Known Limitations" below).  It is provided under the Apache 2.0 license.

## How to Use the Mutation Functionality

Simply add the source files under [runtime](runtime) to your project.  See [ramfuzz-rt.hpp](runtime/ramfuzz-rt.hpp) comments for instructions on how to generate random values, replay logs, and mutate log regions.

## How to Use the Code Generator

The `bin/ramfuzz` executable (in LLVM build, see "how to build" below) generates test code from C++ headers declaring the classes under test.  Instructions for its use are in [main.cpp](main.cpp).

### Known Limitations

C++ is a huge language, so the code generator is a work in progress.  Although improvements are made constantly, it currently can't handle the following important categories:
- templates
- STL containers other than `vector`
- pure virtual methods that return a reference or a pointer (though other aspects of abstract base classes are supported)

It is also worth noting that the random objects produced by the generated code are unconstrained in any way, so they may not adhere to the requirements of code under test.  This may be fine for some tests, but others will likely require some manual adaptation of the generated code.

## How to Build the Code Generator

RamFuzz source is intended to go under `clang/tools/extra` and build from there, as described in [this](http://clang.llvm.org/docs/LibASTMatchersTutorial.html#step-1-create-a-clangtool) clang tutorial (see "Step 1: Create a ClangTool").  Drop the top-level RamFuzz directory into `clang/tools/extra` and add it (using `add_subdirectory`) to `clang/tools/extra/CMakeLists.txt`.  After that, the standard llvm build procedure should produce a `bin/ramfuzz` executable.

There are some end-to-end tests in the [`test`](test) directory -- see [`test.py`](test/test.py) there.  RamFuzz adds a new build target `ramfuzz-test`, which executes all RamFuzz tests.  This target depends on `bin/ramfuzz`, so `bin/ramfuzz` will be rebuilt before testing if it's out of date.

## Contributing

RamFuzz welcomes contributions by the community.  Each contributor will retain copyright over their code but must sign the developer certificate in the [CONTRIBUTORS](CONTRIBUTORS) file by adding their name/contact to the list and using the `-s` flag in all commits.

RamFuzz code relies on extensive Doxygen-style comments to provide guidance to the reader.  Each directory also contains a README file that briefly summarizes what's in it and where to start reading.

To avoid any hurdles to contribution, there is no formal coding style.  Don't worry about formalities, just write good code.  Look it over and ask yourself: is it simple to use, simple to read, and simple to modify?  If so, it's a welcome contribution to this project.
