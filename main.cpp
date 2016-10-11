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
/// ramfuzz <input file> ... [-- <clang option> ...]
///
/// On success, it outputs two files: fuzz.hpp and fuzz.cpp.  They contain the
/// generated test code.  Users can #include fuzz.hpp to get the requisite
/// declarations and compile fuzz.cpp to get an object file with the
/// definitions.  The generator assumes the input files are #includable headers,
/// and fuzz.hpp #includes each of the input files to access the class
/// declarations in the generated code.
///
/// After the "--" argument, ramfuzz takes clang options necessary to parse the
/// input files.  These typically include -I and -std.  (TODO: why is "--" even
/// necessary?  We should be able to take these options directly.)
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
Generates C++ source code that randomly invokes public code from the input
files.  This is useful for unit tests that leverage fuzzing to exercise the code
under test in unexpected ways.  Parameter fuzzing = ramfuzz.
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
  return ramfuzz(Tool, sources, outh, outc);
}
