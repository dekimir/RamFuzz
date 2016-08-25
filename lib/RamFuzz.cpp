#include "RamFuzz.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace ast_matchers;

using clang::tooling::ClangTool;
using clang::tooling::FrontendActionFactory;
using clang::tooling::newFrontendActionFactory;
using std::ostream;
using std::ostringstream;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;

namespace {

bool skip(CXXMethodDecl *M) {
  return isa<CXXDestructorDecl>(M) || M->getAccess() != AS_public ||
         !M->isInstance();
}

auto ClassMatcher =
    cxxRecordDecl(isExpansionInMainFile(),
                  unless(hasAncestor(namespaceDecl(isAnonymous()))),
                  hasDescendant(cxxMethodDecl(isPublic())))
        .bind("class");

/// Generates ramfuzz code into an ostream.  The user can feed a RamFuzz
/// instance to a custom MatchFinder, or simply getActionFactory() and run it in
/// a ClangTool.
class RamFuzz : public MatchFinder::MatchCallback {
public:
  RamFuzz(std::ostream &outh, std::ostream &outc) : outh(outh), outc(outc) {
    MF.addMatcher(ClassMatcher, this);
    AF = newFrontendActionFactory(&MF);
  }

  /// Match callback.
  void run(const MatchFinder::MatchResult &Result) override;

  FrontendActionFactory &getActionFactory() { return *AF; }

  /// Prefix for RamFuzz class names. A RamFuzz class name is this prefix plus
  /// the name of class under test.
  static const string rfcls_prefix;

private:
  /// Generates the declaration and definition of member ctr_roulette.
  void
  gen_ctr_roulette(const string &cls,     ///< Name of class under test.
                   const string &qualcls, ///< Like cls but fully qualified.
                   const string &rfcls,   ///< Name of RamFuzz class.
                   unsigned size          ///< Size of ctr_roulette.
                   );

  /// Generates the declaration and definition of member meth_roulette.
  void gen_meth_roulette(
      const string &rfcls, ///< Name of RamFuzz class.
      const unordered_map<string, unsigned>
          &namecount ///< Method-name histogram of the class under test.
      );

  /// Generates the declaration and definition of a RamFuzz class constructor
  /// from an int.  This constructor internally creates the object under test
  /// using a constructor indicated by the int.
  void gen_int_ctr(const string &rfcls ///< Name of RamFuzz class.
                   );

  /// Where to output generated declarations (typically a header file).
  ostream &outh;

  /// Where to output generated code (typically a C++ source file).
  ostream &outc;

  /// A FrontendActionFactory to run MF.  Owned by *this because it
  /// requires live MF to remain valid.
  shared_ptr<FrontendActionFactory> AF;

  /// A MatchFinder to run *this on ClassMatcher.  Owned by *this
  /// because it's only valid while *this is alive.
  MatchFinder MF;
};

/// Valid identifier from a CXXMethodDecl name.
string valident(const string &mname) {
  static const unordered_map<char, char> table = {
      {' ', '_'}, {'=', 'e'}, {'+', 'p'}, {'-', 'm'}, {'*', 's'},
      {'/', 'd'}, {'%', 'c'}, {'&', 'a'}, {'|', 'f'}, {'^', 'r'},
      {'<', 'l'}, {'>', 'g'}, {'~', 't'}, {'!', 'b'}, {'[', 'h'},
      {']', 'i'}, {'(', 'j'}, {')', 'k'}, {'.', 'n'},
  };
  string transf = mname;
  for (char &c : transf) {
    auto found = table.find(c);
    if (found != table.end())
      c = found->second;
  }
  return transf;
}

} // anonymous namespace

const string RamFuzz::rfcls_prefix = "RF__";

void RamFuzz::gen_ctr_roulette(const string &cls, const string &qualcls,
                               const string &rfcls, unsigned size) {
  outh << "  using cptr = " << qualcls << "* (" << rfcls << "::*)();\n";
  outh << "  static const cptr ctr_roulette[" << size << "];\n";

  outc << "const " << rfcls << "::cptr " << rfcls << "::ctr_roulette[] = {\n  ";
  for (unsigned i = 0; i < size; ++i)
    outc << (i ? ", " : "") << "&" << rfcls << "::" << cls << i;
  outc << "\n};\n";
}

void RamFuzz::gen_meth_roulette(
    const string &rfcls, const unordered_map<string, unsigned> &namecount) {
  const string cls = rfcls.substr(rfcls_prefix.size());
  unsigned meth_roulette_size = 0;
  outc << "const " << rfcls << "::mptr " << rfcls
       << "::meth_roulette[] = {\n  ";
  bool firstel = true;
  for (const auto &nc : namecount) {
    if (nc.first == cls)
      continue; // Skip methods corresponding to constructors under test.
    for (unsigned i = 0; i < nc.second; ++i) {
      if (!firstel)
        outc << ", ";
      firstel = false;
      outc << "&" << rfcls << "::" << nc.first << i;
      meth_roulette_size++;
    }
  }
  outc << "\n};\n";

  outh << "  using mptr = void (" << rfcls << "::*)();\n";
  outh << "  static const mptr meth_roulette[" << meth_roulette_size << "];\n";
}

void RamFuzz::gen_int_ctr(const string &rfcls) {
  outh << "  // Creates obj internally, using indicated constructor.\n";
  outh << "  " << rfcls << "(unsigned ctr);\n";
  outc << rfcls << "::" << rfcls << "(unsigned ctr)\n";
  outc << "  : pobj((this->*ctr_roulette[ctr])()), obj(*pobj) {}\n";
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
    unordered_map<string, unsigned> namecount;
    unsigned mcount = 0;
    const string cls = C->getQualifiedNameAsString();
    const string rfcls = rfcls_prefix + C->getNameAsString();
    outh << "class " << rfcls << " {\n";
    outh << " private:\n";
    outh << "  // Owns internally created objects. Must precede obj "
            "declaration.\n";
    outh << "  std::unique_ptr<" << cls << "> pobj;\n";
    outh << " public:\n";
    outh << "  " << cls << "& obj; // Object under test.\n";
    outh << "  " << rfcls << "(" << cls << "& obj) \n";
    outh << "    : obj(obj) {} // Object already created by caller.\n";
    bool ctrs = false;
    for (auto M : C->methods()) {
      if (skip(M))
        continue;
      const string name = valident(M->getNameAsString());
      if (isa<CXXConstructorDecl>(M)) {
        outh << "  " << cls << "* ";
        outc << cls << "* ";
        ctrs = true;
      } else {
        outh << "  void ";
        outc << "void ";
        mcount++;
      }
      outh << name << namecount[name] << "();\n";
      outc << rfcls << "::" << name << namecount[name] << "() {\n";
      if (isa<CXXConstructorDecl>(M))
        outc << "  return 0;\n";
      outc << "}\n";
      namecount[name]++;
    }
    gen_meth_roulette(rfcls, namecount);
    if (ctrs) {
      gen_int_ctr(rfcls);
      gen_ctr_roulette(C->getNameAsString(), cls, rfcls,
                       namecount[C->getNameAsString()]);
    }
    outh << "};\n\n";
  }
}

string ramfuzz(const string &code) {
  ostringstream strh, strc;
  bool success = clang::tooling::runToolOnCode(
      RamFuzz(strh, strc).getActionFactory().create(), code);
  return success ? strh.str() + strc.str() : "fail";
}

int ramfuzz(ClangTool &tool, const vector<string> &sources, ostream &outh,
            ostream &outc) {
  outh << "#include <memory>\n";
  for (const auto &f : sources)
    outh << "#include \"" << f << "\"\n";
  outh << "\nnamespace ramfuzz {\n\n";
  outc << "\nnamespace ramfuzz {\n\n";
  const int runres = tool.run(&RamFuzz(outh, outc).getActionFactory());
  outc << "} // namespace ramfuzz\n";
  outh << "} // namespace ramfuzz\n";
  return runres;
}
