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

#include "RamFuzz.hpp"

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <tuple>
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
using std::inserter;
using std::make_tuple;
using std::set;
using std::string;
using std::tie;
using std::tuple;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace clang {
// So we can compare SmallVectors of QualType.
bool operator<(const QualType a, const QualType b) {
  return a.getAsOpaquePtr() < b.getAsOpaquePtr();
}
} // namespace clang

namespace {

auto ClassMatcher =
    cxxRecordDecl(isExpansionInMainFile(),
                  unless(hasAncestor(namespaceDecl(isAnonymous()))),
                  hasDescendant(cxxMethodDecl(isPublic())))
        .bind("class");

/// Holds a method's name and signature.  Useful for comparing methods in a
/// subclass with its super class to find overrides and covariants.
struct MethodNameAndSignature {
  using Types = SmallVector<QualType, 8>;
  StringRef name;
  Types params;
  MethodNameAndSignature(const CXXMethodDecl &M)
      : name(M.getName()), params(getTypes(M.parameters())) {}
  bool operator<(const MethodNameAndSignature &that) const {
    if (this->name == that.name)
      return this->params < that.params;
    else
      return this->name < that.name;
  }

private:
  Types getTypes(ArrayRef<ParmVarDecl *> params) {
    Types ts;
    for (auto p : params)
      ts.push_back(p->getOriginalType());
    return ts;
  }
};

/// Generates ramfuzz code into an ostream.  The user can feed a RamFuzz
/// instance to a custom MatchFinder, or simply getActionFactory() and run it in
/// a ClangTool.  Afterwards, the user must call finish().
class RamFuzz : public MatchFinder::MatchCallback {
public:
  RamFuzz(raw_ostream &outh, raw_ostream &outc)
      : outh(outh), outc(outc), prtpol((LangOptions())) {
    MF.addMatcher(ClassMatcher, this);
    AF = newFrontendActionFactory(&MF);
    prtpol.Bool = 1;
    prtpol.SuppressUnwrittenScope = true;
    prtpol.SuppressTagKeyword = true;
    prtpol.SuppressScope = false;
  }

  /// Match callback.
  void run(const MatchFinder::MatchResult &Result) override;

  FrontendActionFactory &getActionFactory() { return *AF; }

  /// Calculates which classes under test need their RamFuzz control but don't
  /// have it yet.  This happens when a control class is referenced in RamFuzz
  /// output code, but its generation hasn't been triggered.
  vector<string> missingClasses();

  /// Emits aditional code required for correct compilation.
  void finish();

private:
  /// If C is abstract, generates an inner class that's a concrete subclass of
  /// C.
  void gen_concrete_impl(const CXXRecordDecl *C, const ASTContext &ctx);

  /// Generates concrete methods of C's concrete_impl class -- one for each pure
  /// method of C and its transitive bases.  Skips any methods present in
  /// to_skip.  Extends to_skip with generated methods.
  void gen_concrete_methods(const CXXRecordDecl *C, const ASTContext &ctx,
                            const string &ns,
                            set<MethodNameAndSignature> &to_skip);

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
                  const char *genname, ///< Name of the runtime::gen member.
                  const Twine &loc,    ///< Code location for logging purposes.
                  const Twine &failval ///< Value to return in early exit.
                  );

  /// Generates the definition of RamFuzz method named rfname, corresponding to
  /// the method under test M.  Assumes that the return type of the generated
  /// method has already been output.
  void gen_method(const Twine &rfname, ///< Fully qualified RamFuzz method name.
                  const CXXMethodDecl *M, const ASTContext &ctx);

  /// Generates the declaration and definition of the runtime::gen::set_any
  /// specialization for class cls in namespace ns.
  void gen_set_any(const string &cls, const string &ns);

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

  /// Qualified names of classes under test that were referenced in generated
  /// code.
  set<string> referenced_classes;

  /// Qualified names of classes under test whose control classes have been
  /// generated.
  set<string> processed_classes;

  /// Enum types for which parameters have been generated.  Maps the enum name
  /// to its values.
  unordered_map<string, vector<string>> referenced_enums;
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

class rfstream;
raw_ostream &operator<<(raw_ostream &, const rfstream &);

/// A streaming adapter for QualType.  Prints C++ code that compiles correctly
/// in the RamFuzz context (unlike Clang's TypePrinter:
/// https://github.com/dekimir/RamFuzz/issues/1).
class rfstream {
public:
  rfstream(const QualType &ty, const PrintingPolicy &prtpol)
      : ty(ty), prtpol(prtpol) {}

  void print(raw_ostream &os) const {
    if (auto el = ty->getAs<ElaboratedType>()) {
      print_cv(os);
      os << rfstream(el->desugar(), prtpol);
    } else if (auto spec = ty->getAs<TemplateSpecializationType>()) {
      print_cv(os);
      print(os, spec->getTemplateName());
      bool first = true;
      for (auto arg : spec->template_arguments()) {
        os << (first ? '<' : ',') << ' '; // Space after < avoids <:.
        if (arg.getKind() == TemplateArgument::Type)
          os << rfstream(arg.getAsType(), prtpol);
        else
          arg.print(prtpol, os);
        first = false;
      }
      os << '>';
    } else if (auto td = ty->getAs<TypedefType>()) {
      td->getDecl()->printQualifiedName(os, prtpol);
    } else if (ty->isReferenceType()) {
      os << rfstream(ty.getNonReferenceType(), prtpol) << '&';
      // TODO: handle lvalue references.
    } else if (ty->isPointerType()) {
      os << rfstream(ty->getPointeeType(), prtpol) << '*';
    } else
      ty.print(os, prtpol);
    // TODO: make this fully equivalent to TypePrinter, handling all possible
    // types or cleverly deferring to it.
  }

  void print_cv(raw_ostream &os) const {
    if (ty.isLocalConstQualified())
      os << "const ";
    if (ty.isLocalVolatileQualified())
      os << "volatile ";
  }

  void print(raw_ostream &os, const TemplateName &name) const {
    if (auto decl = name.getAsTemplateDecl())
      decl->printQualifiedName(os, prtpol);
    else
      name.print(os, prtpol);
  }

  void print(raw_ostream &os, const NestedNameSpecifier &qual) const {
    if (qual.getPrefix())
      qual.getPrefix()->print(os, prtpol);
    switch (qual.getKind()) {
    case NestedNameSpecifier::Identifier:
      os << qual.getAsIdentifier()->getName();
      break;
    case NestedNameSpecifier::Namespace:
      if (qual.getAsNamespace()->isAnonymousNamespace())
        return;
      os << qual.getAsNamespace()->getName();
      break;
    case NestedNameSpecifier::NamespaceAlias:
      os << qual.getAsNamespaceAlias()->getName();
      break;
    case NestedNameSpecifier::Global:
      break;
    case NestedNameSpecifier::TypeSpecWithTemplate:
      os << "template "; // Fallthrough!
    case NestedNameSpecifier::TypeSpec:
      os << rfstream(QualType(qual.getAsType(), 0), prtpol);
      break;
    default:
      break;
    }
    os << "::";
  }

private:
  const QualType &ty;
  const PrintingPolicy &prtpol;
};

raw_ostream &operator<<(raw_ostream &os, const rfstream &thiz) {
  thiz.print(os);
  return os;
}

/// Given a (possibly qualified) class name, returns its constructor's name.
const char *ctrname(const string &cls) {
  const auto found = cls.rfind("::");
  return &cls[found == string::npos ? 0 : found + 2];
}

/// True iff C is visible outside all its parent contexts.
bool globally_visible(const CXXRecordDecl *C) {
  if (!C || !C->getIdentifier())
    // Anonymous classes may technically be visible, but only through tricks
    // like decltype.  Skip until there's a compelling use-case.
    return false;
  const auto acc = C->getAccess();
  if (acc == AS_private || acc == AS_protected)
    return false;
  const DeclContext *ctx = C->getLookupParent();
  while (!isa<TranslationUnitDecl>(ctx)) {
    if (auto ns = dyn_cast<NamespaceDecl>(ctx)) {
      if (ns->isAnonymousNamespace())
        return false;
      ctx = ns->getLookupParent();
      continue;
    } else
      return globally_visible(dyn_cast<CXXRecordDecl>(ctx));
  }
  return true;
}

/// Returns ty's pointee (and if that's a pointer, its pointee, and so on
/// recursively), as well as the depth level of that recursion.
tuple<QualType, unsigned> ultimate_pointee(QualType ty, const ASTContext &ctx) {
  ty = ty.getNonReferenceType().getDesugaredType(ctx);
  unsigned indir_cnt = 0;
  while (ty->isPointerType()) {
    ty = ty->getPointeeType().getNonReferenceType().getDesugaredType(ctx);
    ++indir_cnt;
  }
  return make_tuple(ty, indir_cnt);
}

} // anonymous namespace

vector<string> RamFuzz::missingClasses() {
  vector<string> diff;
  set_difference(referenced_classes.cbegin(), referenced_classes.cend(),
                 processed_classes.cbegin(), processed_classes.cend(),
                 inserter(diff, diff.begin()));
  return diff;
}

void RamFuzz::gen_concrete_methods(const CXXRecordDecl *C,
                                   const ASTContext &ctx, const string &ns,
                                   set<MethodNameAndSignature> &to_skip) {
  if (!C)
    return;
  for (auto M : C->methods()) {
    if (M->isPure() && !to_skip.count(MethodNameAndSignature(*M))) {
      to_skip.insert(*M);
      const auto bg = M->param_begin(), en = M->param_end();
      const auto mcom = [bg](decltype(bg) &P) { return P == bg ? "" : ", "; };
      auto Mrty = rfstream(M->getReturnType(), prtpol);
      outh << "    " << Mrty << " " << *M << "(";
      outc << Mrty << " " << ns << "::control::concrete_impl::" << *M << "(";
      for (auto P = bg; P != en; ++P) {
        auto Pty = rfstream((*P)->getType(), prtpol);
        outh << mcom(P) << Pty;
        outc << mcom(P) << Pty;
      }
      outh << ") " << (M->isConst() ? "const " : "") << "override;\n";
      outc << ") " << (M->isConst() ? "const " : "") << "{\n";
      auto rety =
          M->getReturnType().getDesugaredType(ctx).getLocalUnqualifiedType();
      if (rety->isPointerType()) {
        const auto pty = rety->getPointeeType();
        if (pty->isScalarType()) {
          const auto s = pty.stream(prtpol);
          outc << "  return new " << s << "(ramfuzzgenuniquename.any<" << s
               << ">());\n";
        } else if (const auto ptcls = pty->getAsCXXRecordDecl()) {
          gen_object(ptcls, "rfctl", "ramfuzzgenuniquename",
                     Twine(ns) + "::concrete_impl::" + M->getName(), "nullptr");
          outc << "  return rfctl.release();\n";
        } else if (pty->isVoidType()) {
          outc << "  auto rfctl = "
                  "runtime::spin_roulette<rfstd_vector::control<char>>("
                  "ramfuzzgenuniquename);\n";
          outc << "  return rfctl.obj.data();\n";
        } else
          assert(0 && "TODO: handle other types.");
      } else if (rety->isReferenceType()) {
        const auto deref = rety.getNonReferenceType();
        if (const auto retcls = deref->getAsCXXRecordDecl()) {
          outc << "  static std::unique_ptr<" << rfstream(deref, prtpol)
               << "> global;\n";
          outc << "  // Spin roulette locally, since it can call us "
                  "recursively.\n";
          gen_object(retcls, "local", "ramfuzzgenuniquename",
                     Twine(ns) + "::concrete_impl::" + M->getName(), "*global");
          outc << "  // Transfer to global avoids dangling reference.\n";
          outc << "  global.reset(local.release());\n";
          outc << "  return *global;\n";
        } else
          assert(0 && "TODO: handle other types.");
      } else if (rety->isScalarType()) {
        outc << "  return ramfuzzgenuniquename.any<" << rfstream(rety, prtpol)
             << ">();\n";
      } else if (const auto retcls = rety->getAsCXXRecordDecl()) {
        gen_object(retcls, "rfctl", "ramfuzzgenuniquename",
                   Twine(ns) + "::concrete_impl::" + M->getName(), "rfctl.obj");
        outc << "  return rfctl.obj;\n";
      } else
        assert(rety->isVoidType() && "TODO: handle other types.");
      outc << "}\n\n";
    }
  }
  for (const auto &base : C->bases())
    gen_concrete_methods(base.getType()->getAsCXXRecordDecl(), ctx, ns,
                         to_skip);
}

void RamFuzz::gen_concrete_impl(const CXXRecordDecl *C, const ASTContext &ctx) {
  if (C->isAbstract()) {
    const auto cls = C->getQualifiedNameAsString();
    const auto ns = control_namespace(cls);
    outh << "  struct concrete_impl : public " << cls;
    outh << " {\n";
    outh << "    runtime::gen& ramfuzzgenuniquename;\n";
    for (const auto M : C->ctors()) {
      if (M->getAccess() != AS_private) {
        outh << "    concrete_impl(runtime::gen& ramfuzzgenuniquename";
        const auto bg = M->param_begin(), en = M->param_end();
        const auto mcom = [bg](decltype(bg) &P) { return P == bg ? "" : ", "; };
        for (auto P = bg; P != en; ++P)
          outh << ", " << rfstream((*P)->getType(), prtpol) << " p"
               << P - bg + 1;
        C->printQualifiedName(outh << ") \n      : ");
        outh << "(";
        for (auto P = bg; P != en; ++P)
          outh << mcom(P) << "p" << P - bg + 1;
        outh << "), ramfuzzgenuniquename(ramfuzzgenuniquename) {}\n";
      }
    }
    if (C->needsImplicitDefaultConstructor()) {
      outh << "    concrete_impl(runtime::gen& ramfuzzgenuniquename)\n";
      outh << "      : ramfuzzgenuniquename(ramfuzzgenuniquename) {}\n";
    }
    set<MethodNameAndSignature> skip_nothing;
    gen_concrete_methods(C, ctx, ns, skip_nothing);
    outh << "  };\n";
  }
}

void RamFuzz::gen_croulette(const string &cls, unsigned size) {
  const auto ns = control_namespace(cls);
  outh << "  using cptr = " << cls << "* (control::*)();\n";
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
  outc << "    std::cout << \"" << loc << " exiting early due to " << reason
       << "\" << std::endl;\n";
  outc << "    --calldepth;\n";
  outc << "    return " << failval << ";\n";
}

void RamFuzz::gen_object(const CXXRecordDecl *cls, const Twine &varname,
                         const char *genname, const Twine &loc,
                         const Twine &failval) {
  const auto ctl = control(cls, prtpol);
  outc << "  auto " << varname << " = runtime::spin_roulette<" << ctl << ">("
       << genname << ");\n";
  if (!cls->isInStdNamespace())
    referenced_classes.insert(cls->getQualifiedNameAsString());
  outc << "  if (!" << varname << ") {\n";
  early_exit(loc, failval, Twine("failed ") + varname + " constructor");
  outc << "  }\n";
}

void RamFuzz::gen_method(const Twine &rfname, const CXXMethodDecl *M,
                         const ASTContext &ctx) {
  outc << rfname << "() {\n";
  outc << "  if (++calldepth >= depthlimit) {\n";
  early_exit(rfname, isa<CXXConstructorDecl>(M) ? "nullptr" : "",
             "call depth limit");
  outc << "  }\n";
  SmallVector<unsigned, 8> ptrcnt(M->param_size() + 1);
  auto ramcount = 0u;
  for (const auto &ram : M->parameters()) {
    ramcount++;
    // Type of the generated variable:
    auto vartype = ram->getType()
                       .getNonReferenceType()
                       .getDesugaredType(ctx)
                       .getLocalUnqualifiedType();
    tie(vartype, ptrcnt[ramcount]) = ultimate_pointee(vartype, ctx);
    if (vartype->isScalarType()) {
      outc << "  " << vartype.stream(prtpol) << " ram" << ramcount
           << " = g.any<" << vartype.stream(prtpol) << ">();\n";
    } else if (const auto varcls = vartype->getAsCXXRecordDecl()) {
      const auto rfvar = Twine("rfram") + Twine(ramcount);
      gen_object(varcls, rfvar, "g", rfname,
                 isa<CXXConstructorDecl>(M) ? "nullptr" : "");
      outc << "  auto& ram" << ramcount << " = " << rfvar << ".obj;\n";
    } else if (vartype->isVoidType()) {
      assert(ptrcnt[ramcount]); // Must've been a void*.
      outc << "  auto rfram" << ramcount
           << " = runtime::spin_roulette<rfstd_vector::control<char>>(g);\n";
      outc << "  " << (vartype.isLocalConstQualified() ? "const " : "")
           << "void* ram" << ramcount << " = rfram" << ramcount
           << ".obj.data();\n";
      // Because the ram variable is already a pointer, one less indirection is
      // needed below.
      ptrcnt[ramcount]--;
    }
    if (const auto et = vartype->getAs<EnumType>()) {
      const auto decl = et->getDecl();
      const string name = decl->getQualifiedNameAsString();
      for (const auto c : decl->enumerators())
        referenced_enums[name].push_back(c->getQualifiedNameAsString());
    }
    for (auto i = 0u; i < ptrcnt[ramcount]; ++i) {
      outc << "  auto ram" << ramcount << "p" << i << " = std::addressof(ram"
           << ramcount;
      if (i)
        outc << "p" << i - 1;
      outc << ");\n";
    }
  }
  if (isa<CXXConstructorDecl>(M)) {
    outc << "  --calldepth;\n";
    outc << "  return new ";
    if (M->getParent()->isAbstract())
      outc << "concrete_impl(g" << (ramcount ? ", " : "");
    else {
      M->getParent()->printQualifiedName(outc);
      outc << "(";
    }
  } else
    outc << "  obj." << *M << "(";
  for (auto i = 1u; i <= ramcount; ++i) {
    outc << (i == 1 ? "" : ", ") << "ram" << i;
    if (ptrcnt[i])
      outc << "p" << ptrcnt[i] - 1;
  }
  outc << ");\n";
  if (!isa<CXXConstructorDecl>(M))
    outc << "  --calldepth;\n";
  outc << "}\n\n";
}

void RamFuzz::gen_set_any(const string &cls, const string &ns) {
  outh << "template <> void runtime::gen::set_any<" << cls << ">(" << cls
       << "&);\n";
  outc << "template <> void runtime::gen::set_any<" << cls << ">(" << cls
       << "&obj) {\n";
  outc << "  auto ctl = runtime::spin_roulette<" << ns
       << "::control>(*this);\n";
  outc << "  assign(ctl, obj);\n";
  outc << "}\n";
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
    if (!globally_visible(C) || C->getDescribedClassTemplate() ||
        isa<ClassTemplateSpecializationDecl>(C))
      return;
    const string cls = C->getQualifiedNameAsString();
    const string ns = control_namespace(cls);
    outh << "namespace " << ns << " {\n";
    outh << "class control {\n";
    outh << " private:\n";
    outh << "  runtime::gen& g; // Declare first to initialize early; "
            "constructors may use it.\n";
    outh << "  // Owns internally created objects. Must precede obj "
            "declaration.\n";
    outh << "  std::unique_ptr<" << cls << "> pobj;\n";
    // Call depth should be made atomic when we start supporting multi-threaded
    // fuzzing.  Holding off for now because we expect to get a lot of mileage
    // out of multi-process fuzzing (running multiple fuzzing executables, each
    // in its own process).  That should still keep all the hardware occupied
    // without paying for the overhead of thread-safety.
    outh << "  // Prevents infinite recursion.\n";
    outh << "  static unsigned calldepth;\n";
    outc << "unsigned " << ns << "::control::calldepth = 0;\n\n";
    outh << "  static const unsigned depthlimit = "
            "ramfuzz::runtime::depthlimit;\n";
    gen_concrete_impl(C, *Result.Context);
    outh << " public:\n";
    outh << "  " << cls << "& obj; // Object under test.\n";
    outh << "  control(runtime::gen& g, " << cls
         << "& obj) : g(g), obj(obj) {} // Object already created by caller.\n";
    outh << "  // True if obj was successfully internally created.\n";
    outh << "  operator bool() const { return bool(pobj); }\n";
    outh << "  // Releases internally generated object and returns its address "
            "\n  // (or null if externally generated).\n";
    outh << "  " << cls << "* release() { return pobj.release(); }\n";
    unordered_map<string, unsigned> namecount;
    bool ctrs = false;
    for (auto M : C->methods()) {
      if (isa<CXXDestructorDecl>(M) || M->getAccess() != AS_public ||
          !M->isInstance() || M->isDeleted())
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
      gen_method(Twine(ns) + "::control::" + name + Twine(namecount[name]), M,
                 *Result.Context);
      namecount[name]++;
    }
    if (C->needsImplicitDefaultConstructor()) {
      const auto name = ctrname(cls);
      outh << "  " << cls << "* ";
      outh << name << namecount[name]++ << "() { return new ";
      if (C->isAbstract())
        outh << "concrete_impl(g)";
      else
        outh << cls << "()";
      outh << "; }\n";
      ctrs = true;
    }
    gen_mroulette(ns, C->getName(), namecount);
    if (ctrs) {
      gen_int_ctr(ns);
      gen_croulette(cls, namecount[C->getNameAsString()]);
    } else {
      outh << "  // No public constructors; user should implement this:\n";
      outh << "  static control make(runtime::gen& g);\n";
    }
    outh << "};\n";
    outh << "}; // namespace " << ns << "\n";
    if (ctrs)
      gen_set_any(cls, ns);
    outc << "\n";
    processed_classes.insert(cls);
  }
}

void RamFuzz::finish() {
  for (auto e : referenced_enums) {
    outh << "template<> " << e.first << " ramfuzz::runtime::gen::any<"
         << e.first << ">();\n";
    outc << "template<> " << e.first << " ramfuzz::runtime::gen::any<"
         << e.first << ">() {\n";
    outc << "  static " << e.first << " a[] = {\n    ";
    int comma = 0;
    for (const auto &n : e.second)
      outc << (comma++ ? "," : "") << n;
    outc << "  };\n";
    outc
        << "  return a[between(std::size_t(0), sizeof(a)/sizeof(a[0]) - 1)];\n";
    outc << "}\n";
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
            raw_ostream &outc, raw_ostream &errs) {
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
  RamFuzz rf(outh, outc);
  const int run_error = tool.run(&rf.getActionFactory());
  rf.finish();
  outc << "} // namespace ramfuzz\n";
  outh << "} // namespace ramfuzz\n";
  if (run_error)
    return 1;
  const auto missing = rf.missingClasses();
  if (!missing.empty()) {
    errs << "RamFuzz code will likely not compile, because the following "
            "required classes \nwere not processed:\n";
    for (const auto cls : missing)
      errs << cls << '\n';
    return 2;
  }
  return 0;
}
