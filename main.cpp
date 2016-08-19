#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "lib/RamFuzz.hpp"
#include "llvm/Support/CommandLine.h"

using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using clang::tooling::newFrontendActionFactory;
using llvm::cl::OptionCategory;
using llvm::cl::extrahelp;

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
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  return Tool.run(newFrontendActionFactory(&RamFuzz().getMatchFinder()).get());
}
