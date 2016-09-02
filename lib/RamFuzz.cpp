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
using llvm::raw_ostream;
using llvm::raw_string_ostream;
using std::ostringstream;
using std::to_string;
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
  RamFuzz(raw_ostream &outh, raw_ostream &outc)
      : outh(outh), outc(outc), prtpol((LangOptions())) {
    MF.addMatcher(ClassMatcher, this);
    AF = newFrontendActionFactory(&MF);
    prtpol.Bool = 1;
  }

  /// Match callback.
  void run(const MatchFinder::MatchResult &Result) override;

  FrontendActionFactory &getActionFactory() { return *AF; }

  /// Prefix for RamFuzz class names. A RamFuzz class name is this prefix plus
  /// the name of class under test.
  static const string rfcls_prefix;

private:
  /// Generates the declaration and definition of member croulette.
  void gen_croulette(const string &cls,     ///< Name of class under test.
                     const string &qualcls, ///< Like cls but fully qualified.
                     const string &rfcls,   ///< Name of RamFuzz class.
                     unsigned size          ///< Size of croulette.
                     );

  /// Generates the declaration and definition of member mroulette.
  void gen_mroulette(
      const string &rfcls, ///< Name of RamFuzz class.
      const unordered_map<string, unsigned>
          &namecount ///< Method-name histogram of the class under test.
      );

  /// Generates the declaration and definition of a RamFuzz class constructor
  /// from an int.  This constructor internally creates the object under test
  /// using a constructor indicated by the int.
  void gen_int_ctr(const string &rfcls ///< Name of RamFuzz class.
                   );

  /// Generates early exit from RamFuzz method named \c rfname, corresponding to
  /// the method under test \c M.  The exit code prints \c reason as the reason
  /// for exiting early.  The exit code is multiple statements, so the caller
  /// may need to generate a pair of braces around it.
  void early_exit(const string &rfname, const CXXMethodDecl *M,
                  const string &reason);

  /// Generates the definition of RamFuzz method named rfname, corresponding to
  /// the method under test M.  Assumes that the return type of the generated
  /// method has already been output.
  void gen_method(const string &rfname, const CXXMethodDecl *M,
                  const ASTContext &ctx);

  /// Where to output generated declarations (typically a header file).
  raw_ostream &outh;

  /// Where to output generated code (typically a C++ source file).
  raw_ostream &outc;

  /// A FrontendActionFactory to run MF.  Owned by *this because it
  /// requires live MF to remain valid.
  shared_ptr<FrontendActionFactory> AF;

  /// A MatchFinder to run *this on ClassMatcher.  Owned by *this
  /// because it's only valid while *this is alive.
  MatchFinder MF;

  /// Policy for printing to outh and outc.
  PrintingPolicy prtpol;
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

void RamFuzz::gen_croulette(const string &cls, const string &qualcls,
                            const string &rfcls, unsigned size) {
  outh << "  using cptr = " << qualcls << "* (" << rfcls << "::*)();\n";
  outh << "  static const cptr croulette[" << size << "];\n";
  outh << "  static constexpr unsigned ccount = " << size << ";\n";

  outc << "const " << rfcls << "::cptr " << rfcls << "::croulette[] = {\n  ";
  for (unsigned i = 0; i < size; ++i)
    outc << (i ? ", " : "") << "&" << rfcls << "::" << cls << i;
  outc << "\n};\n";
}

void RamFuzz::gen_mroulette(const string &rfcls,
                            const unordered_map<string, unsigned> &namecount) {
  const string cls = rfcls.substr(rfcls_prefix.size());
  unsigned mroulette_size = 0;
  outc << "const " << rfcls << "::mptr " << rfcls << "::mroulette[] = {\n  ";
  bool firstel = true;
  for (const auto &nc : namecount) {
    if (nc.first == cls)
      continue; // Skip methods corresponding to constructors under test.
    for (unsigned i = 0; i < nc.second; ++i) {
      if (!firstel)
        outc << ", ";
      firstel = false;
      outc << "&" << rfcls << "::" << nc.first << i;
      mroulette_size++;
    }
  }
  outc << "\n};\n";

  outh << "  using mptr = void (" << rfcls << "::*)();\n";
  outh << "  static const mptr mroulette[" << mroulette_size << "];\n";
  outh << "  static constexpr unsigned mcount = " << mroulette_size << ";\n";
}

void RamFuzz::gen_int_ctr(const string &rfcls) {
  outh << "  // Creates obj internally, using indicated constructor.\n";
  outh << "  explicit " << rfcls << "(unsigned ctr);\n";
  outc << rfcls << "::" << rfcls << "(unsigned ctr)\n";
  outc << "  : pobj((this->*croulette[ctr])()), obj(*pobj) {}\n";
}

void RamFuzz::early_exit(const string &rfname, const CXXMethodDecl *M,
                         const string &reason) {
  outc << "    std::cout << \"" << rfname << " exiting early due to " << reason
       << "\" << std::endl;\n";
  outc << "    return" << (isa<CXXConstructorDecl>(M) ? " 0" : "") << ";\n";
}

void RamFuzz::gen_method(const string &rfname, const CXXMethodDecl *M,
                         const ASTContext &ctx) {
  outc << rfname << "() {\n";
  outc << "  if (calldepth >= depthlimit) {\n";
  early_exit(rfname, M, "call depth limit");
  outc << "  }\n";
  outc << "  ++calldepth;\n";
  auto ramcount = 0u;
  for (const auto &ram : M->parameters()) {
    ramcount++;
    // Type of the generated variable:
    auto vartype = ram->getType()
                       .getNonReferenceType()
                       .getDesugaredType(ctx)
                       .getLocalUnqualifiedType();
    if (vartype->isScalarType()) {
      vartype.print(outc << "  ", prtpol);
      vartype.print(outc << " ram" << ramcount << " = g.any<", prtpol);
      outc << ">(\"" << rfname << "::ram" << ramcount << "\");\n";
    } else if (const auto varcls = vartype->getAsCXXRecordDecl()) {
      const auto rfvarcls = rfcls_prefix + varcls->getNameAsString();
      const auto rfvar = string("rfram") + to_string(ramcount);
      const auto rfvarid = rfname + "::" + rfvar;
      outc << "  " << rfvarcls << " " << rfvar << "(g.between(0u, " << rfvarcls
           << "::ccount-1,\"" << rfvarid << "-croulette\"));\n";
      outc << "  if (!" << rfvar << ") {\n";
      early_exit(rfname, M, "failed " + rfvar + " constructor");
      outc << "  }\n";
      outc << "  const auto mspins" << ramcount
           << " = g.between(0u, ::ramfuzz::runtime::spinlimit, \"" << rfvarid
           << "-mspins\");\n";
      outc << "  for (auto i = 0u; i < mspins" << ramcount << "; ++i)\n";
      outc << "    (" << rfvar << ".*" << rfvar << ".mroulette[g.between(0u, "
           << rfvarcls << "::mcount-1, \"" << rfvarid << "-m\")])();\n";
      outc << "  auto ram" << ramcount << " = " << rfvar << ".obj;\n";
    }
  }
  if (isa<CXXConstructorDecl>(M)) {
    outc << "  --calldepth;\n";
    M->getParent()->printQualifiedName(outc << "  return new ");
  } else
    M->printName(outc << "  obj.");
  outc << "(";
  for (auto i = 1u; i <= ramcount; ++i)
    outc << (i == 1 ? "" : ", ") << "ram" << i;
  outc << ");\n";
  if (!isa<CXXConstructorDecl>(M))
    outc << "  --calldepth;\n";
  outc << "}\n";
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
    unordered_map<string, unsigned> namecount;
    const string cls = C->getQualifiedNameAsString();
    const string rfcls = rfcls_prefix + C->getNameAsString();
    outh << "class " << rfcls << " {\n";
    outh << " private:\n";
    outh << "  // Owns internally created objects. Must precede obj "
            "declaration.\n";
    outh << "  std::unique_ptr<" << cls << "> pobj;\n";
    outh << "  runtime::gen g;\n";
    // Call depth should be made atomic when we start supporting multi-threaded
    // fuzzing.  Holding off for now because we expect to get a lot of mileage
    // out of multi-process fuzzing (running multiple fuzzing executables, each
    // in its own process).  That should still keep all the hardware occupied
    // without paying the overhead of thread-safety.
    outh << "  // Prevents infinite recursion.\n";
    outh << "  static unsigned calldepth;\n";
    outc << "  unsigned " << rfcls << "::calldepth = 0;\n";
    outh << "  static const unsigned depthlimit = "
            "::ramfuzz::runtime::depthlimit;\n";
    outh << " public:\n";
    outh << "  " << cls << "& obj; // Object under test.\n";
    outh << "  " << rfcls << "(" << cls << "& obj) \n";
    outh << "    : obj(obj) {} // Object already created by caller.\n";
    outh << "  // True if obj was successfully internally created.\n";
    outh << "  operator bool() const { return bool(pobj); }\n";
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
      }
      outh << name << namecount[name] << "();\n";
      gen_method(rfcls + "::" + name + to_string(namecount[name]), M,
                 *Result.Context);
      namecount[name]++;
    }
    gen_mroulette(rfcls, namecount);
    if (ctrs) {
      gen_int_ctr(rfcls);
      gen_croulette(C->getNameAsString(), cls, rfcls,
                    namecount[C->getNameAsString()]);
    }
    outh << "};\n\n";
  }
}

string ramfuzz(const string &code) {
  string hpp, cpp;
  raw_string_ostream ostrh(hpp), ostrc(cpp);
  bool success = clang::tooling::runToolOnCode(
      RamFuzz(ostrh, ostrc).getActionFactory().create(), code);
  return success ? ostrh.str() + ostrc.str() : "fail";
}

int ramfuzz(ClangTool &tool, const vector<string> &sources, raw_ostream &outh,
            raw_ostream &outc) {
  outh << "#include <memory>\n";
  for (const auto &f : sources)
    outh << "#include \"" << f << "\"\n";
  outh << "#include \"ramfuzz-rt.hpp\"\n";
  outh << "\nnamespace ramfuzz {\n\n";
  outc << "#include <cstddef>\n";
  outc << "#include <iostream>\n\n";
  outc << "\nnamespace ramfuzz {\n\n";
  const int runres = tool.run(&RamFuzz(outh, outc).getActionFactory());
  outc << "} // namespace ramfuzz\n";
  outh << "} // namespace ramfuzz\n";
  return runres;
}
