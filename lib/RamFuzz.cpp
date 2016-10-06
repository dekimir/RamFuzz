#include "RamFuzz.hpp"

#include <memory>
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
    prtpol.SuppressUnwrittenScope = true;
    prtpol.SuppressTagKeyword = true;
  }

  /// Match callback.
  void run(const MatchFinder::MatchResult &Result) override;

  FrontendActionFactory &getActionFactory() { return *AF; }

private:
  /// If C is abstract, generates an inner class that's a concrete subclass of
  /// C.
  void gen_concrete_impl(const CXXRecordDecl *C, const ASTContext &ctx);

  /// Generates the declaration and definition of member croulette.
  void gen_croulette(
      const string &cls, ///< Fully qualified name of class under test.
      unsigned size      ///< Size of croulette.
      );

  /// Generates the declaration and definition of member mroulette.
  void gen_mroulette(
      const string &ns,      ///< Namespace for the RamFuzz control class.
      const StringRef &name, ///< Name of class under test.
      const unordered_map<string, unsigned>
          &namecount ///< Method-name histogram of the class under test.
      );

  /// Generates the declaration and definition of a RamFuzz class constructor
  /// from an int.  This constructor internally creates the object under test
  /// using a constructor indicated by the int.
  void
  gen_int_ctr(const string &ns ///< Namespace for the RamFuzz control class.
              );

  /// Generates early exit from RamFuzz method \c name, corresponding to the
  /// method under test \c M.  The exit code prints \c reason as the reason for
  /// exiting early, then returns \c failval.  The exit code is multiple
  /// statements, so the caller may need to generate a pair of braces around it.
  void early_exit(const Twine &loc,     ///< Code location for logging purposes.
                  const Twine &failval, ///< Value to return in early exit.
                  const Twine &reason   ///< Exit reason, for logging purposes.
                  );

  /// Generates an instance of the RamFuzz control class for cls, naming that
  /// instance varname.
  void gen_object(const CXXRecordDecl *cls, ///< Class under test.
                  const Twine &varname,     ///< Name of the generated variable.
                  const Twine &loc,    ///< Code location for logging purposes.
                  const Twine &failval ///< Value to return in early exit.
                  );

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

/// Returns the namespace for cls's control class.
string control_namespace(const string &cls) {
  string transf = "rf";
  for (const char &c : cls)
    if (c != ':')
      transf.push_back(c);
    else if (transf.back() != '_')
      transf.push_back('_');
  return transf;
}

/// Makes a reference to cls's control class.
string control(const CXXRecordDecl *cls, const PrintingPolicy &prtpol) {
  std::string QualName;
  raw_string_ostream OS(QualName);
  cls->printQualifiedName(OS, prtpol);
  auto ctlstr = control_namespace(OS.str());
  raw_string_ostream ctl(ctlstr);
  ctl << "::control";
  if (auto tmpl = dyn_cast<ClassTemplateSpecializationDecl>(cls)) {
    ctl << '<';
    bool first = true;
    for (auto arg : tmpl->getTemplateArgs().asArray()) {
      if (!first)
        ctl << ", ";
      first = false;
      auto ty = arg.getAsType();
      if (ty.isNull())
        arg.print(prtpol, ctl);
      else
        ctl << ty.stream(prtpol);
    }
    ctl << '>';
  }
  return ctl.str();
}

/// Given a (possibly qualified) class name, returns its constructor's name.
const char *ctrname(const string &cls) {
  const auto found = cls.rfind("::");
  return &cls[found == string::npos ? 0 : found + 2];
}

} // anonymous namespace

void RamFuzz::gen_concrete_impl(const CXXRecordDecl *C, const ASTContext &ctx) {
  if (C->isAbstract()) {
    const auto cls = C->getQualifiedNameAsString();
    const auto ns = control_namespace(cls);
    outh << "  struct concrete_impl : public ::" << cls;
    outh << " {\n";
    outh << "    runtime::gen& g;\n";
    for (auto M : C->methods()) {
      const auto bg = M->param_begin(), en = M->param_end();
      const auto mcom = [bg](decltype(bg) &P) { return P == bg ? "" : ", "; };
      if (M->isPure()) {
        outh << "    " << M->getReturnType().stream(prtpol) << " " << *M << "(";
        outc << M->getReturnType().stream(prtpol) << " " << ns
             << "::control::concrete_impl::" << *M << "(";
        for (auto P = bg; P != en; ++P) {
          outh << mcom(P) << (*P)->getType().stream(prtpol);
          outc << mcom(P) << (*P)->getType().stream(prtpol);
        }
        outh << ") " << (M->isConst() ? "const " : "") << "override;\n";
        outc << ") " << (M->isConst() ? "const " : "") << "{\n";
        auto rety =
            M->getReturnType().getDesugaredType(ctx).getLocalUnqualifiedType();
        if (rety->isScalarType()) {
          outc << "  return g.any<" << rety.stream(prtpol) << ">();\n";
        } else if (const auto retcls = rety->getAsCXXRecordDecl()) {
          gen_object(retcls, "rfctl",
                     Twine(ns) + "::concrete_impl::" + M->getName(),
                     "rfctl.obj");
          outc << "  return rfctl.obj;\n";
        }
        // TODO: handle other types.
        outc << "}\n";
      } else if (isa<CXXConstructorDecl>(M) && M->getAccess() != AS_private) {
        outh << "    concrete_impl(runtime::gen& g";
        for (auto P = bg; P != en; ++P)
          outh << ", " << (*P)->getType().stream(prtpol) << " p" << P - bg + 1;
        C->printQualifiedName(outh << ") : ::");
        outh << "(";
        for (auto P = bg; P != en; ++P)
          outh << mcom(P) << "p" << P - bg + 1;
        outh << "), g(g) {}\n";
      }
    }
    if (C->needsImplicitDefaultConstructor())
      outh << "    concrete_impl(runtime::gen& g) : g(g) {}\n";
    outh << "  };\n";
  }
}

void RamFuzz::gen_croulette(const string &cls, unsigned size) {
  const auto ns = control_namespace(cls);
  outh << "  using cptr = ::" << cls << "* (control::*)();\n";
  outh << "  static constexpr unsigned ccount = " << size << ";\n";
  outh << "  static const cptr croulette[ccount];\n";

  outc << "const " << ns << "::control::cptr " << ns
       << "::control::croulette[] = {\n  ";

  for (unsigned i = 0; i < size; ++i)
    outc << (i ? ", " : "") << "&" << ns << "::control::" << ctrname(cls) << i;
  outc << "\n};\n";
}

void RamFuzz::gen_mroulette(const string &ns, const StringRef &name,
                            const unordered_map<string, unsigned> &namecount) {
  unsigned mroulette_size = 0;
  outc << "const " << ns << "::control::mptr " << ns
       << "::control::mroulette[] = {\n  ";
  bool firstel = true;
  for (const auto &nc : namecount) {
    if (nc.first == name)
      continue; // Skip methods corresponding to constructors under test.
    for (unsigned i = 0; i < nc.second; ++i) {
      if (!firstel)
        outc << ", ";
      firstel = false;
      outc << "&" << ns << "::control::" << nc.first << i;
      mroulette_size++;
    }
  }
  outc << "\n};\n";

  outh << "  using mptr = void (control::*)();\n";
  outh << "  static constexpr unsigned mcount = " << mroulette_size << ";\n";
  outh << "  static const mptr mroulette[mcount];\n";
}

void RamFuzz::gen_int_ctr(const string &ns) {
  outh << "  // Creates obj internally, using indicated constructor.\n";
  outh << "  control(runtime::gen& g, unsigned ctr);\n";
  outc << ns << "::control::control(runtime::gen& g, unsigned ctr)\n";
  outc << "  : g(g), pobj((this->*croulette[ctr])()), obj(*pobj) {}\n";
}

void RamFuzz::early_exit(const Twine &loc, const Twine &failval,
                         const Twine &reason) {
  outc << "    ::std::cout << \"" << loc << " exiting early due to " << reason
       << "\" << ::std::endl;\n";
  outc << "    --calldepth;\n";
  outc << "    return " << failval << ";\n";
}

void RamFuzz::gen_object(const CXXRecordDecl *cls, const Twine &varname,
                         const Twine &loc, const Twine &failval) {
  prtpol.SuppressTagKeyword = true;
  const auto ctl = control(cls, prtpol);
  outc << "  " << ctl << " " << varname << " = runtime::spin_roulette<" << ctl
       << ">(g);\n";
  outc << "  if (!" << varname << ") {\n";
  early_exit(loc, failval, Twine("failed ") + varname + " constructor");
  outc << "  }\n";
  prtpol.SuppressTagKeyword = false;
}

void RamFuzz::gen_method(const Twine &rfname, const CXXMethodDecl *M,
                         const ASTContext &ctx) {
  outc << rfname << "() {\n";
  outc << "  if (++calldepth >= depthlimit) {\n";
  early_exit(rfname, isa<CXXConstructorDecl>(M) ? "nullptr" : "",
             "call depth limit");
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
      outc << "  " << vartype.stream(prtpol) << " ram" << ramcount
           << " = g.any<" << vartype.stream(prtpol) << ">();\n";
    } else if (const auto varcls = vartype->getAsCXXRecordDecl()) {
      const auto rfvar = Twine("rfram") + Twine(ramcount);
      gen_object(varcls, rfvar, rfname,
                 isa<CXXConstructorDecl>(M) ? "nullptr" : "");
      outc << "  auto& ram" << ramcount << " = " << rfvar << ".obj;\n";
    }
  }
  if (isa<CXXConstructorDecl>(M)) {
    outc << "  --calldepth;\n";
    outc << "  return new ";
    if (M->getParent()->isAbstract())
      outc << "concrete_impl(g" << (ramcount ? ", " : "");
    else {
      M->getParent()->printQualifiedName(outc << "::");
      outc << "(";
    }
  } else
    outc << "  obj." << *M << "(";
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
    const string ns = control_namespace(cls);
    outh << "namespace " << ns << " {\n";
    outh << "class control {\n";
    outh << " private:\n";
    outh << "  runtime::gen& g; // Declare first to initialize early; "
            "constructors may use it.\n";
    outh << "  // Owns internally created objects. Must precede obj "
            "declaration.\n";
    outh << "  ::std::unique_ptr<::" << cls << "> pobj;\n";
    // Call depth should be made atomic when we start supporting multi-threaded
    // fuzzing.  Holding off for now because we expect to get a lot of mileage
    // out of multi-process fuzzing (running multiple fuzzing executables, each
    // in its own process).  That should still keep all the hardware occupied
    // without paying for the overhead of thread-safety.
    outh << "  // Prevents infinite recursion.\n";
    outh << "  static unsigned calldepth;\n";
    outc << "unsigned " << ns << "::control::calldepth = 0;\n\n";
    outh << "  static const unsigned depthlimit = "
            "::ramfuzz::runtime::depthlimit;\n";
    gen_concrete_impl(C, *Result.Context);
    outh << " public:\n";
    outh << "  ::" << cls << "& obj; // Object under test.\n";
    outh << "  control(runtime::gen& g, ::" << cls
         << "& obj) : g(g), obj(obj) {} // Object already created by caller.\n";
    outh << "  // True if obj was successfully internally created.\n";
    outh << "  operator bool() const { return bool(pobj); }\n";
    unordered_map<string, unsigned> namecount;
    bool ctrs = false;
    for (auto M : C->methods()) {
      if (isa<CXXDestructorDecl>(M) || M->getAccess() != AS_public ||
          !M->isInstance())
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
      gen_method(Twine(ns) + "::control::" + name + Twine(namecount[name]), M,
                 *Result.Context);
      namecount[name]++;
    }
    if (C->needsImplicitDefaultConstructor()) {
      const auto name = ctrname(cls);
      outh << "  ::" << cls << "* ";
      outh << name << namecount[name]++ << "() { return new ";
      if (C->isAbstract())
        outh << "concrete_impl(g)";
      else
        outh << "::" << cls << "()";
      outh << "; }\n";
      ctrs = true;
    }
    gen_mroulette(ns, C->getName(), namecount);
    if (ctrs) {
      gen_int_ctr(ns);
      gen_croulette(cls, namecount[C->getNameAsString()]);
    }
    outh << "};\n";
    outh << "}; // namespace " << ns << "\n";
    outh << "template <> void runtime::gen::set_any<::" << cls << ">(::" << cls
         << "&);\n";
    outc << "template <> void runtime::gen::set_any<::" << cls << ">(::" << cls
         << "&obj) {\n";
    outc << "  auto ctl = runtime::spin_roulette<" << ns
         << "::control>(*this);\n";
    outc << "  if (ctl) obj = ctl.obj;\n";
    outc << "}\n\n";
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
  outc << R"(#include <cstddef>
#include <iostream>
#include <string>

namespace ramfuzz {

)";
  const int runres = tool.run(&RamFuzz(outh, outc).getActionFactory());
  outc << "} // namespace ramfuzz\n";
  outh << "} // namespace ramfuzz\n";
  return runres;
}
