#include <iostream>
#include <string>
#include <unordered_map>

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;

using std::cout;
using std::ostream;
using std::string;
using std::unordered_map;

auto ClassMatcher =
    cxxRecordDecl(isExpansionInMainFile(),
                  unless(hasAncestor(namespaceDecl(isAnonymous()))),
                  hasDescendant(cxxMethodDecl(isPublic())))
        .bind("class");

namespace {
bool skip(CXXMethodDecl *m) { return isa<CXXDestructorDecl>(m); }
} // anonymous namespace

class ClassPrinter : public MatchFinder::MatchCallback {
public:
  ClassPrinter(ostream &out = cout) : out(out) {}

  void run(const MatchFinder::MatchResult &Result) override {
    if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
      unordered_map<string, unsigned> namecount;
      out << "namespace ramfuzz {\n";
      out << "class RF__" << C->getNameAsString() << " {\n";
      out << "  " << C->getQualifiedNameAsString() << "& obj;\n";
      out << " public:\n";
      for (auto M : C->methods()) {
        if (skip(M))
          continue;
        const string name = M->getNameAsString();
        out << "  void " << name << namecount[name]++ << "() {\n";
        out << "  }\n";
      }
      out << "};\n";
      out << "} // namespace ramfuzz\n";
    }
  }

private:
  ostream &out;
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("ramfuzz options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::extrahelp RamFuzzHelp(R"(
Generates C++ source code that randomly invokes public code from the input
files.  This is useful for unit tests that leverage fuzzing to exercise the code
under test in unexpected ways.  Parameter fuzzing = ramfuzz.
)");

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  MatchFinder Finder;
  ClassPrinter CP;
  Finder.addMatcher(ClassMatcher, &CP);
  return Tool.run(newFrontendActionFactory(&Finder).get());
}
