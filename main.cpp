// Copyright 2016 The RamFuzz contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// This file contains the main() function for the ramfuzz executable, the
/// RamFuzz code generator.  This executable scans C++ files for class
/// declarations and produces test code that can create random objects of these
/// classes.  The invocation syntax is
///
/// ramfuzz <input file> ... -- [<clang option> ...]
///
/// On success, it outputs two files: fuzz.hpp and fuzz.cpp.  They contain the
/// generated test code.  Users #include fuzz.hpp to get the requisite
/// declarations; they compile fuzz.cpp to get an object file with the
/// definitions.  The generator assumes the input files are #includable headers,
/// and fuzz.hpp #includes each of the input files to access the class
/// declarations in the generated code.
///
/// After the "--" argument, ramfuzz takes clang options necessary to parse the
/// input files.  These typically include -I, -std, and -xc++ (to force .h files
/// to be treated as C++ instead of C).
///
/// (TODO: why is "--" necessary?  We should be able to take all relevant
/// options directly.)
///
/// For every class in an input file (but not in other headers #included from
/// input files), it generates a _control class_ in the ramfuzz namespace.  The
/// control class is in charge of creating an instance of the class under test
/// and invoking its methods with random parameter values.  The control class is
/// named to encode the fully qualified name of the class under test.  For
/// example, if the class under test is a::b::C, then the control class will be
/// named ramfuzz::rfa_b_C::control.  See test/namespace.cpp for real-life
/// examples.
///
/// The control class has one method for each public non-static method of the
/// class under test.  The control method, when invoked, generates random
/// arguments and invokes the corresponding method under test.  Control methods
/// take no arguments, as they are self-contained and generate parameter values
/// internally.  The control class also has a constructor that constructs the
/// object under test using any one of its public constructors.  So a random
/// object under test may be obtained by constructing the control class and then
/// randomly invoking its methods to exercise the under-test instance.  This is
/// called spinning the method roulette.  See the function spin_roulette() in
/// runtime/ramfuzz-rt.hpp for an example of this process.
///
/// Exit code is 0 on success, 1 on a Clang-reported error, and 2 if more input
/// is needed to generate full testing code.  For explanation of 2, consider
/// this code on input:
///
/// class Foo;
/// struct Bar { void process_foo(Foo& foo); };
///
/// Upon seeing the process_foo() declaration, ramfuzz will generate code to
/// exercise this method with a random Foo value.  But that code will reference
/// Foo's control class ramfuzz::rfFoo::control.  If ramfuzz doesn't see Foo's
/// definition, its control class will never get generated, so Bar's control
/// will fail to compile with an error like "use of undeclared identifier
/// 'rfFoo'".  Since ramfuzz keeps track of whether Foo's control is generated
/// or not, it can detect this situation and return the exit code 2 to warn the
/// user that the generated code is incomplete.  In that case, ramfuzz will also
/// print to standard error a list of classes whose controls are missing.
///
/// Keep in mind that ramfuzz only generates code for its input files and not
/// for other files #included from them.  It is thus possible to get exit status
/// 2 if Foo's definition exists but is #included.  The remedy is to add Foo's
/// header to the list of ramfuzz input files.

#include <iostream>
#include <system_error>

#include "lib/RamFuzz.hpp"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"

using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using llvm::cl::OptionCategory;
using llvm::cl::extrahelp;
using llvm::raw_fd_ostream;
using llvm::sys::fs::OpenFlags;
using std::cerr;
using std::endl;
using std::error_code;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static OptionCategory MyToolCategory("ramfuzz options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static extrahelp RamFuzzHelp(R"(
Generates test code that creates random instances of classes defined in input
files.  This is useful for unit tests that wish to fuzz parameter values for
code under test.  Parameter fuzzing = ramfuzz.

Outputs fuzz.hpp and fuzz.cpp with the declarations and definitions of test
code.
)");

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  const auto &sources = OptionsParser.getSourcePathList();
  ClangTool Tool(OptionsParser.getCompilations(), sources);
  error_code ec;
  raw_fd_ostream outh("fuzz.hpp", ec, OpenFlags::F_Text);
  if (ec) {
    cerr << "Cannot open fuzz.hpp: " << ec.message() << endl;
    return 1;
  }
  raw_fd_ostream outc("fuzz.cpp", ec, OpenFlags::F_Text);
  if (ec) {
    cerr << "Cannot open fuzz.cpp: " << ec.message() << endl;
    return 1;
  }
  outc << "#include \"fuzz.hpp\"\n";
  return ramfuzz::genTests(Tool, sources, outh, outc, llvm::errs());
}
