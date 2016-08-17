#include <iostream>
#include <string>
#include <unordered_map>

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;

using std::cout;
using std::endl;
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
  void run(const MatchFinder::MatchResult &Result) override {
    if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
      unordered_map<string, unsigned> namecount;
      cout << "namespace ramfuzz {\n";
      cout << "class RF__" << C->getNameAsString() << " {\n";
      cout << "  " << C->getQualifiedNameAsString() << "& obj;\n";
      cout << " public:\n";
      for (auto M : C->methods()) {
        if (skip(M))
          continue;
        const string name = M->getNameAsString();
        cout << "  void " << name << namecount[name]++ << "() {\n";
        cout << "  }\n";
      }
      cout << "};\n";
      cout << "} // namespace ramfuzz\n";
    }
  }
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  MatchFinder Finder;
  ClassPrinter CP;
  Finder.addMatcher(ClassMatcher, &CP);
  return Tool.run(newFrontendActionFactory(&Finder).get());
}
