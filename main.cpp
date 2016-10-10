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
