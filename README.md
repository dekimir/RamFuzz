# RamFuzz: Combining Unit Tests, Fuzzing, and AI

RamFuzz is a fuzzer for individual method parameters in unit tests.  A unit test can use RamFuzz to generate random parameter values for methods under test.  The values are logged, and the log can be replayed to repeat the exact same test scenario.  But random parameter values aren't limited to just fundamental types: RamFuzz can also automatically produce random objects of any class from the user's code. This allows the user to fuzz methods that accept class parameters. For example, if a method takes an `int`, a `struct S`, and a `class C` as parameters, RamFuzz can randomly generate them all!  The test writer needn't worry about the technicalities of creating `S` and `C` objects; RamFuzz will do it automatically.

To accomplish this, RamFuzz includes a code generator that reads the C++ source code and generates from it some test code. This test code creates a class instance via a randomly chosen constructor, then proceeds to invoke a random sequence of instance methods with randomly generated arguments. The result is a random class object that can be fed to any method taking that class as a paremeter.

Of course, this produces superficial tests that likely aren't very useful on average -- most methods don't take completely random parameters but constrain them in some way.  The intent is that these constraints can be inferred automatically from the logs of many RamFuzz test runs.  Because tests are randomized, each run is a different scenario that adds coverage of the code under test.  If the quality of randomness is good, then running tests repeatedly for a long time will cover a wide range of possible parameter values, likely including some perfectly valid tests.  These logs can then be used, for example, to train an AI to recognize which parameter values are valid.  Alternatively, fuzzing strategies can be used to steer the evolution of parameter-value generation towards valid tests.

RamFuzz provides some Python tools to make it easy to train AI on the logs it generates -- see the [`ai`](ai) directory and [the overview paper](sci/ramfuzz.md).  It is possible, for instance, to train a neural network to accurately predict the outcome of a test run based on its RamFuzz log.  This is possible because RamFuzz manages to provide test runs sufficiently varied to be useful for discerning what succeeds and what does not.

Note, however, that RamFuzz doesn't automatically generate test assertions on the results of method invocations.  Other projects will be created to accomplish that automatically, but RamFuzz will make it possible by conveniently handling parameter fuzzing and logging.

RamFuzz is provided under the Apache 2.0 license.  It currently supports only C++ on input (please see "Known Limitations" below), and it requires C++11 support for compiling its output code and runtime.

## How to Use

The `bin/ramfuzz` executable (in LLVM build, see "How to Build" below) generates test code from C++ headers declaring the classes under test.  Say we have a header file `a.hpp` with the following contents:

```c++
class Base {
public:
  unsigned sum = 0;
  void bump(unsigned delta) { sum += delta; }
  virtual char id() const { return 'B'; }
};

class Sub : public Base {
public:
  char id() const override { return 'S'; }
};
```

We feed this header to `bin/ramfuzz` like this:
```sh
~/src/llvmbuild/bin/ramfuzz a.hpp -- -std=c++11
```

The result is two files named `fuzz.hpp` and `fuzz.cpp`.  These, together with the [RamFuzz runtime](runtime), contain code that can generate random objects of class `Base`.  The interface is simple: just invoke `runtime::gen::make<Base>()`, and you get an object of type `Base`.  Here is a program to demonstrate it:
```c++
#include <iostream>
#include "fuzz.hpp"

int main(int argc, char *argv[]) {
  ramfuzz::runtime::gen g(argc, argv);
  for (;;) {
    const auto b = g.make<Base>(g.or_subclass);
    std::cout << b->id() << b->sum << std::endl;
  }
}

unsigned ramfuzz::runtime::spinlimit = 2;
```

Including `fuzz.hpp` brings in all the required declarations, including the RamFuzz runtime.  The `runtime::gen` object keeps RNG state, manages logging, and provides the `make()` method for creating random values of any type.  It is documented in [runtime/ramfuzz-rt.hpp](runtime/ramfuzz-rt.hpp).  The above program simply generates random `Base` objects and prints them out in an infinite loop.

Say the above code is in a file named `main.cpp` in the same directory as `fuzz.*` and `ramfuzz-rt.*`.  Then we can compile it like this:
```sh
 c++ -std=c++11 main.cpp fuzz.cpp ramfuzz-rt.cpp 
```

Here's an excerpt from the resulting executable's output:
```
B0
B1034171753
B390426976
S0
B1608827789
B1581714349
S0
B277714725
B1526711793
S0
B0
```

Note that `make<Base>(or_subclass)` sometimes produces a `B` object and sometimes an `S` one.  The created objects will have their methods (including `bump()`) invoked a random number of times in random order with random arguments -- this is how a random `Base` is created.

As the executable runs, it logs the random numbers generated into a file named `fuzzlog`.  And this log can be replayed by running the executable again with `fuzzlog` as the command-line argument -- that will execute the same code paths and print the same output again.

You can see more examples in the [test](test) directory, where each `.hpp` file is processed by `bin/ramfuzz` and the result linked with the eponymous `.cpp` file during testing.

### Known Limitations

C++ is a huge language, so the code generator is a work in progress.  Although improvements are made constantly, it currently can't handle the following important categories:
- template parameters with default values
- variadic templates
- STL containers other than `vector` and `string`
- array parameters
- function pointers
- parameter values that must equal some method's return value

These limitations will typically manifest themselves as ill-formed C++ on the output.

## How to Build

1. **Get Clang:** the RamFuzz code generator is a Clang tool, so first get and build Clang using [these instructions](http://clang.llvm.org/get_started.html).  RamFuzz is intended to work with Clang trunk and is periodically updated to be compatible with it.  This has been ongoing since release 3.8.1, the first release RamFuzz worked with.

2. **Drop RamFuzz into Clang:** RamFuzz source is intended to go under `clang/tools/extra` and build from there (as described in [this](http://clang.llvm.org/docs/LibASTMatchersTutorial.html#step-1-create-a-clangtool) Clang tutorial).  Drop the top-level RamFuzz directory into `clang/tools/extra` and add it (using `add_subdirectory`) to `clang/tools/extra/CMakeLists.txt`.

3. **Rebuild Clang:** Now the standard LLVM build procedure should produce a `bin/ramfuzz` executable.

4. **Run Tests:** There are some end-to-end tests in the [`test`](test) directory -- see [`test.py`](test/test.py) there.  There are also unit tests in the [`unittests`](unittests) directory.  RamFuzz adds a new build target `check-ramfuzz`, which executes all unit- and end-to-end tests.  The end-to-end tests depend on `bin/ramfuzz`, so `bin/ramfuzz` will be rebuilt before testing if it's out of date.

## How to Contribute

RamFuzz welcomes contributions by the community.  Each contributor will retain copyright over their code but must sign the developer certificate in the [CONTRIBUTORS](CONTRIBUTORS) file by adding their name/contact to the list and using the `-s` flag in all commits.

RamFuzz code relies on extensive Doxygen-style comments to provide guidance to the reader.  Each directory also contains a README file that briefly summarizes what's in it and where to start reading.

To avoid any hurdles to contribution, there is no formal coding style.  Don't worry about formalities, just write good code.  Look it over and ask yourself: is it simple to use, simple to read, and simple to modify?  If so, it's a welcome contribution to this project.
