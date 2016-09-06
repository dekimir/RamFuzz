#include "RamFuzz.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/SmallBitVector.h"

using namespace clang;
using namespace ast_matchers;

using clang::tooling::ClangTool;
using clang::tooling::FrontendActionFactory;
using clang::tooling::newFrontendActionFactory;
using llvm::SmallBitVector;
using llvm::raw_ostream;
using llvm::raw_string_ostream;
using std::string;
using std::unique_ptr;
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

private:
  /// Generates the declaration and definition of member croulette.
  void gen_croulette(
      const string &cls, ///< Fully qualified name of class under test.
      unsigned size      ///< Size of croulette.
      );

  /// Generates the declaration and definition of member mroulette.
  void gen_mroulette(
      const string &cls, ///< Qualified name of the class under test.
      const unordered_map<string, unsigned>
          &namecount ///< Method-name histogram of the class under test.
      );

  /// Generates the declaration and definition of a RamFuzz class constructor
  /// from an int.  This constructor internally creates the object under test
  /// using a constructor indicated by the int.
  void
  gen_int_ctr(const string &cls ///< Qualified name of the class under test.
              );

  /// Generates early exit from RamFuzz method \c name, corresponding to the
  /// method under test \c M.  The exit code prints \c reason as the reason for
  /// exiting early.  The exit code is multiple statements, so the caller may
  /// need to generate a pair of braces around it.
  void early_exit(const Twine &name, ///< Name of the exiting method.
                  const CXXMethodDecl *M, const Twine &reason);

  /// Generates the definition of RamFuzz method named rfname, corresponding to
  /// the method under test M.  Assumes that the return type of the generated
  /// method has already been output.
  void gen_method(const Twine &rfname, ///< Fully qualified RamFuzz method name.
                  const CXXMethodDecl *M, const ASTContext &ctx);

  /// Where to output generated declarations (typically a header file).
  raw_ostream &outh;

  /// Where to output generated code (typically a C++ source file).
  raw_ostream &outc;

  /// A FrontendActionFactory to run MF.  Owned by *this because it
  /// requires live MF to remain valid.
  unique_ptr<FrontendActionFactory> AF;

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

/// Given a (possibly qualified) class name, returns its constructor's name.
const char *ctrname(const string &cls) {
  const auto found = cls.rfind("::");
  return &cls[found == string::npos ? 0 : found + 2];
}

/// Returns the first character that's not a colon or s.cend().
string::const_iterator first_noncolon(const string &s) {
  auto it = s.cbegin();
  while (it != s.cend() && *it == ':')
    ++it;
  return it;
}

/// Opens one or more namespaces in \c s.  For each part of \c name delimited by
/// "::", prints "namespace " <part of \c name> " { ".  Ends with a newline.
void opennss(const string &name, raw_ostream &s) {
  auto start = first_noncolon(name);
  if (start == name.cend())
    return;
  s << "namespace ";
  for (auto c = start; c != name.cend(); ++c)
    if (*c == ':') {
      s << " { namespace ";
      ++c;
      assert(c != name.cend() && *c == ':');
    } else
      s << *c;
  s << " {\n";
}

/// Closes braces opened by opennss() and adds two newlines.
void closenss(const string &name, raw_ostream &s) {
  auto start = first_noncolon(name);
  if (start == name.cend())
    return;
  s << "}";
  for (auto c = start; c != name.cend(); ++c)
    if (*c == ':') {
      s << "}";
      ++c;
      assert(c != name.cend() && *c == ':');
    }
  s << "\n\n";
}

} // anonymous namespace

void RamFuzz::gen_croulette(const string &cls, unsigned size) {
  outh << "  using cptr = ::" << cls << "* (control::*)();\n";
  outh << "  static constexpr unsigned ccount = " << size << ";\n";
  outh << "  static const cptr croulette[ccount];\n";

  outc << "const " << cls << "::control::cptr " << cls
       << "::control::croulette[] = {\n  ";

  for (unsigned i = 0; i < size; ++i)
    outc << (i ? ", " : "") << "&" << cls << "::control::" << ctrname(cls) << i;
  outc << "\n};\n";
}

void RamFuzz::gen_mroulette(const string &cls,
                            const unordered_map<string, unsigned> &namecount) {
  unsigned mroulette_size = 0;
  outc << "const " << cls << "::control::mptr " << cls
       << "::control::mroulette[] = {\n  ";
  bool firstel = true;
  const auto ctr = ctrname(cls);
  for (const auto &nc : namecount) {
    if (nc.first == ctr)
      continue; // Skip methods corresponding to constructors under test.
    for (unsigned i = 0; i < nc.second; ++i) {
      if (!firstel)
        outc << ", ";
      firstel = false;
      outc << "&" << cls << "::control::" << nc.first << i;
      mroulette_size++;
    }
  }
  outc << "\n};\n";

  outh << "  using mptr = void (control::*)();\n";
  outh << "  static constexpr unsigned mcount = " << mroulette_size << ";\n";
  outh << "  static const mptr mroulette[mcount];\n";
}

void RamFuzz::gen_int_ctr(const string &cls) {
  outh << "  // Creates obj internally, using indicated constructor.\n";
  outh << "  explicit control(unsigned ctr);\n";
  outc << cls << "::control::control(unsigned ctr)\n";
  outc << "  : pobj((this->*croulette[ctr])()), obj(*pobj) {}\n";
}

void RamFuzz::early_exit(const Twine &name, const CXXMethodDecl *M,
                         const Twine &reason) {
  outc << "    std::cout << \"" << name << " exiting early due to " << reason
       << "\" << std::endl;\n";
  outc << "    --calldepth;\n";
  outc << "    return" << (isa<CXXConstructorDecl>(M) ? " 0" : "") << ";\n";
}

void RamFuzz::gen_method(const Twine &rfname, const CXXMethodDecl *M,
                         const ASTContext &ctx) {
  outc << rfname << "() {\n";
  outc << "  if (++calldepth >= depthlimit) {\n";
  early_exit(rfname, M, "call depth limit");
  outc << "  }\n";
  SmallBitVector isptr(M->param_size() + 1);
  auto ramcount = 0u;
  for (const auto &ram : M->parameters()) {
    ramcount++;
    // Type of the generated variable:
    auto vartype = ram->getType()
                       .getNonReferenceType()
                       .getDesugaredType(ctx)
                       .getLocalUnqualifiedType();
    if (vartype->isPointerType()) {
      isptr.set(ramcount);
      vartype = vartype->getPointeeType();
    }
    if (vartype->isScalarType()) {
      vartype.print(outc << "  ", prtpol);
      vartype.print(outc << " ram" << ramcount << " = g.any<", prtpol);
      outc << ">(\"" << rfname << "::ram" << ramcount << "\");\n";
    } else if (const auto varcls = vartype->getAsCXXRecordDecl()) {
      const string cls = varcls->getQualifiedNameAsString();
      const auto rfvar = Twine("rfram") + Twine(ramcount);
      const auto rfvarid = rfname + "::" + rfvar;
      outc << "  " << cls << "::control " << rfvar << "(g.between(0u, " << cls
           << "::control::ccount-1,\"" << rfvarid << "-croulette\"));\n";
      outc << "  if (!" << rfvar << ") {\n";
      early_exit(rfname, M, Twine("failed ") + rfvar + " constructor");
      outc << "  }\n";
      outc << "  const auto mspins" << ramcount
           << " = g.between(0u, ::ramfuzz::runtime::spinlimit, \"" << rfvarid
           << "-mspins\");\n";
      outc << "  if (" << cls << "::control::mcount)\n";
      outc << "    for (auto i = 0u; i < mspins" << ramcount << "; ++i)\n";
      outc << "      (" << rfvar << ".*" << rfvar << ".mroulette[g.between(0u, "
           << cls << "::control::mcount-1, \"" << rfvarid << "-m\")])();\n";
      outc << "  auto ram" << ramcount << " = " << rfvar << ".obj;\n";
    }
  }
  if (isa<CXXConstructorDecl>(M)) {
    outc << "  --calldepth;\n";
    M->getParent()->printQualifiedName(outc << "  return new class ");
  } else
    M->printName(outc << "  obj.");
  outc << "(";
  for (auto i = 1u; i <= ramcount; ++i)
    outc << (i == 1 ? "" : ", ") << (isptr[i] ? "&" : "") << "ram" << i;
  outc << ");\n";
  if (!isa<CXXConstructorDecl>(M))
    outc << "  --calldepth;\n";
  outc << "}\n\n";
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
    const string cls = C->getQualifiedNameAsString();
    // Open namespaces for each part of cls split on "::".  Doesn't matter if
    // the part is a namespace or a class -- in RamFuzz code, they'll all be
    // namespaces.  The RamFuzz control class (that invokes under-test methods
    // with fuzzed parameters) will be named "control" in this nest of
    // namespaces.  That class can now be referenced as cls+"::control".  But
    // the class under test C must now be referenced as "::"+cls, due to C++
    // lookup-resolution rules.
    opennss(cls, outh);
    outh << "class control {\n";
    outh << " private:\n";
    outh << "  // Owns internally created objects. Must precede obj "
            "declaration.\n";
    outh << "  std::unique_ptr<::" << cls << "> pobj;\n";
    outh << "  runtime::gen g;\n";
    // Call depth should be made atomic when we start supporting multi-threaded
    // fuzzing.  Holding off for now because we expect to get a lot of mileage
    // out of multi-process fuzzing (running multiple fuzzing executables, each
    // in its own process).  That should still keep all the hardware occupied
    // without paying for the overhead of thread-safety.
    outh << "  // Prevents infinite recursion.\n";
    outh << "  static unsigned calldepth;\n";
    outc << "unsigned " << cls << "::control::calldepth = 0;\n\n";
    outh << "  static const unsigned depthlimit = "
            "::ramfuzz::runtime::depthlimit;\n";
    outh << " public:\n";
    outh << "  ::" << cls << "& obj; // Object under test.\n";
    outh << "  control(::" << cls
         << "& obj) : obj(obj) {} // Object already created by caller.\n";
    outh << "  // True if obj was successfully internally created.\n";
    outh << "  operator bool() const { return bool(pobj); }\n";
    unordered_map<string, unsigned> namecount;
    bool ctrs = false;
    for (auto M : C->methods()) {
      if (skip(M))
        continue;
      const string name = valident(M->getNameAsString());
      if (isa<CXXConstructorDecl>(M)) {
        outh << "  ::" << cls << "* ";
        outc << "::" << cls << "* ";
        ctrs = true;
      } else {
        outh << "  void ";
        outc << "void ";
      }
      outh << name << namecount[name] << "();\n";
      gen_method(Twine(cls) + "::control::" + name + Twine(namecount[name]), M,
                 *Result.Context);
      namecount[name]++;
    }
    if (C->needsImplicitDefaultConstructor()) {
      const auto name = ctrname(cls);
      outh << "  ::" << cls << "* ";
      outh << name << namecount[name]++ << "() { return new ::" << cls
           << "();}\n";
      ctrs = true;
    }
    gen_mroulette(cls, namecount);
    if (ctrs) {
      gen_int_ctr(cls);
      gen_croulette(cls, namecount[C->getNameAsString()]);
    }
    outh << "};\n";
    closenss(cls, outh);
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
