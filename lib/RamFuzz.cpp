#include "RamFuzz.hpp"

#include <sstream>
#include <string>
#include <unordered_map>

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace ast_matchers;

using clang::tooling::ClangTool;
using clang::tooling::newFrontendActionFactory;
using std::ostream;
using std::ostringstream;
using std::string;
using std::unordered_map;

namespace {

bool skip(CXXMethodDecl *M) { return isa<CXXDestructorDecl>(M); }

auto ClassMatcher =
    cxxRecordDecl(isExpansionInMainFile(),
                  unless(hasAncestor(namespaceDecl(isAnonymous()))),
                  hasDescendant(cxxMethodDecl(isPublic())))
        .bind("class");

} // anonymous namespace

RamFuzz::RamFuzz(ostream &out) : out(out) {}

MatchFinder RamFuzz::makeMatchFinder() {
  MatchFinder MF;
  MF.addMatcher(ClassMatcher, this);
  return MF;
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
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

string ramfuzz(const string &code) {
  ostringstream str;
  RamFuzz RF(str);
  MatchFinder MF = RF.makeMatchFinder();
  bool success = clang::tooling::runToolOnCode(
      newFrontendActionFactory(&MF)->create(), code);
  return success ? str.str() : "fail";
}

int ramfuzz(ClangTool &tool, ostream &out) {
  RamFuzz RF(out);
  auto MF = RF.makeMatchFinder();
  return tool.run(newFrontendActionFactory(&MF).get());
}
