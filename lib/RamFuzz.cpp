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

#include "Inheritance.hpp"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

using namespace std;

using namespace clang;
using namespace ast_matchers;
using namespace tooling;

using llvm::raw_ostream;
using llvm::raw_string_ostream;
using ramfuzz::Inheritance;

namespace clang {
// So we can compare SmallVectors of QualType.
bool operator<(const QualType a, const QualType b) {
  return a.getAsOpaquePtr() < b.getAsOpaquePtr();
}
} // namespace clang

namespace {

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

/// Generates RamFuzz code into an ostream.  The user can tack a RamFuzz
/// instance onto a MatchFinder for running it via a frontend action.  After the
/// frontend action completes, the user must call finish().
class RamFuzz : public MatchFinder::MatchCallback {
public:
  /// Prepares for emitting RamFuzz code into outh and outc.
  RamFuzz(raw_ostream &outh, raw_ostream &outc)
      : outh(outh), outc(outc), prtpol((LangOptions())) {
    prtpol.Bool = 1;
    prtpol.SuppressUnwrittenScope = true;
    prtpol.SuppressTagKeyword = true;
    prtpol.SuppressScope = false;
  }

  /// Match callback.  Expects Result to have a CXXRecordDecl* binding for
  /// "class".
  void run(const MatchFinder::MatchResult &Result) override;

  /// Adds to MF a matcher that will generate RamFuzz code (capturing *this).
  void tackOnto(MatchFinder &MF);

  /// Calculates which classes under test need their harness specialization but
  /// don't have it yet.  This happens when a harness class is referenced in
  /// RamFuzz output code, but its generation hasn't been triggered.
  vector<string> missingClasses();

  /// Emits aditional code required for correct compilation.
  void finish(const Inheritance &);

private:
  /// If C is abstract, generates an inner class that's a concrete subclass of
  /// C.
  void gen_concrete_impl(const CXXRecordDecl *C, const ASTContext &ctx);

  /// If ty is an enum, adds it to referenced_enums.
  void register_enum(const Type &ty);

  /// Generates concrete implementations of all C's (and its transitive bases')
  /// pure methods in cls's concrete_impl class.  C must be either cls or its
  /// base class.  Skips any methods present in to_skip.  Extends to_skip with
  /// generated methods.
  void gen_concrete_methods(
      /// Class whose pure methods to implement (including inherited pure
      /// methods).
      const CXXRecordDecl *C,
      /// Fully qualified name of the class in whose concrete_impl to place
      /// generated methods.  Must be C or its subclass.  Eg, if C=BaseClass,
      /// cls=NS1::SubClass, and BaseClass::method1() is pure, this will
      /// generate a concrete implementation of
      /// harness<NS1::SubClass>::method1().
      const string &cls,
      /// Context for desugaring types.
      const ASTContext &ctx,
      /// Methods to skip, eg, because they've already been generated.  Will be
      /// extended with all C's methods generated in this call.
      set<MethodNameAndSignature> &to_skip);

  /// Generates the declaration and definition of member croulette.
  void gen_croulette(
      const string &cls, ///< Fully qualified name of class under test.
      unsigned size      ///< Size of croulette.
      );

  /// Generates the declaration and definition of member mroulette.
  void gen_mroulette(
      const string &cls, ///< Fully qualified name of class under test.
      const unordered_map<string, unsigned>
          &namecount ///< Method-name histogram of the class under test.
      );

  /// Generates the declaration and definition of a RamFuzz class constructor
  /// from an int.  This constructor internally creates the object under test
  /// using a constructor indicated by the int.
  void
  gen_int_ctr(const string &cls ///< Fully qualified name of class under test.
              );

  /// Generates the declaration of member submakers.
  void gen_submakers_decl(
      const string &cls ///< Fully qualified name of class under test.
      );

  /// Generates the definition of member submakers for each of the classes
  /// processed so far.
  void gen_submakers_defs(const Inheritance &inh);

  /// Generates early exit from RamFuzz method \c name, corresponding to the
  /// method under test \c M.  The exit code prints \c reason as the reason for
  /// exiting early, then returns \c failval.  The exit code is multiple
  /// statements, so the caller may need to generate a pair of braces around it.
  void early_exit(const Twine &loc,     ///< Code location for logging purposes.
                  const Twine &failval, ///< Value to return in early exit.
                  const Twine &reason   ///< Exit reason, for logging purposes.
                  );

  /// True iff M's harness method may recursively call itself.  For example, a
  /// copy constructor's harness needs to construct another object of the same
  /// type, which involves a second harness that may itself call the copy
  /// constructor.  The code will look something like this (assuming class under
  /// test is named Foo):
  ///
  /// Foo* harness<Foo>::Foo123() { return new Foo(*g.make<Foo>()); }
  ///
  /// g.make<Foo>() will create a second harness<Foo> object and possibly invoke
  /// its Foo123() method, so we have the outer Foo123() transitively calling
  /// the inner one -- recursion.  This may go infinitely deep when the wrong
  /// random sequence is generated.
  bool harness_may_recurse(const CXXMethodDecl *M, const ASTContext &ctx);

  /// Generates the definition of harness method named hname, corresponding to
  /// the method under test M.  Assumes that the return type of the generated
  /// method has already been output.
  void gen_method(
      const Twine &hname, ///< Fully qualified harness method name.
      const CXXMethodDecl *M, const ASTContext &ctx,
      bool may_recurse ///< True iff generated body may recursively call itself.
      );

  /// Where to output generated declarations (typically a header file).
  raw_ostream &outh;

  /// Where to output generated code (typically a C++ source file).
  raw_ostream &outc;

  /// Policy for printing to outh and outc.
  PrintingPolicy prtpol;

  /// Qualified names of classes under test that were referenced in generated
  /// code.
  set<string> referenced_classes;

  /// Qualified names of classes under test whose harness specializations have
  /// been generated.
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

/// Streams str n consecutive times to os.
void ntimes(unsigned n, const string &str, raw_ostream &os) {
  while (n--)
    os << str;
}

} // anonymous namespace

vector<string> RamFuzz::missingClasses() {
  vector<string> diff;
  set_difference(referenced_classes.cbegin(), referenced_classes.cend(),
                 processed_classes.cbegin(), processed_classes.cend(),
                 inserter(diff, diff.begin()));
  return diff;
}

void RamFuzz::register_enum(const Type &ty) {
  if (const auto et = ty.getAs<EnumType>()) {
    const auto decl = et->getDecl();
    const string name = decl->getQualifiedNameAsString();
    for (const auto c : decl->enumerators())
      referenced_enums[name].push_back(c->getQualifiedNameAsString());
  }
}

void RamFuzz::gen_concrete_methods(const CXXRecordDecl *C, const string &cls,
                                   const ASTContext &ctx,
                                   set<MethodNameAndSignature> &to_skip) {
  if (!C)
    return;
  for (auto M : C->methods()) {
    if (M->isPure() && !to_skip.count(*M)) {
      to_skip.insert(*M);
      const auto bg = M->param_begin(), en = M->param_end();
      const auto mcom = [bg](decltype(bg) &P) { return P == bg ? "" : ", "; };
      auto Mrty = rfstream(M->getReturnType(), prtpol);
      outh << "    " << Mrty << " " << *M << "(";
      outc << Mrty << " harness<" << cls << ">::concrete_impl::" << *M << "(";
      for (auto P = bg; P != en; ++P) {
        auto Pty = rfstream((*P)->getType(), prtpol);
        outh << mcom(P) << Pty;
        outc << mcom(P) << Pty;
      }
      outh << ") " << (M->isConst() ? "const " : "") << "override;\n";
      outc << ") " << (M->isConst() ? "const " : "") << "{\n";
      auto rety =
          M->getReturnType().getDesugaredType(ctx).getLocalUnqualifiedType();
      if (!rety->isVoidType()) {
        outc << "  return *ramfuzzgenuniquename.make<"
             << rfstream(rety.getNonReferenceType().getUnqualifiedType(),
                         prtpol)
             << ">("
             << (rety->isPointerType() || rety->isReferenceType() ? "true" : "")
             << ");\n";
        register_enum(*get<0>(ultimate_pointee(rety, ctx)));
      }
      outc << "}\n\n";
    }
  }
  for (const auto &base : C->bases())
    gen_concrete_methods(base.getType()->getAsCXXRecordDecl(), cls, ctx,
                         to_skip);
}

void RamFuzz::gen_concrete_impl(const CXXRecordDecl *C, const ASTContext &ctx) {
  if (C->isAbstract()) {
    const auto cls = C->getQualifiedNameAsString();
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
    gen_concrete_methods(C, C->getQualifiedNameAsString(), ctx, skip_nothing);
    outh << "  };\n";
  }
}

void RamFuzz::gen_croulette(const string &cls, unsigned size) {
  outh << "  using cptr = " << cls << "* (harness::*)();\n";
  outh << "  static constexpr unsigned ccount = " << size << ";\n";
  outh << "  static const cptr croulette[ccount];\n";

  outc << "const harness<" << cls << ">::cptr harness<" << cls
       << ">::croulette[] = {\n  ";
  for (unsigned i = 0; i < size; ++i)
    outc << (i ? ", " : "") << "&harness<" << cls << ">::" << ctrname(cls) << i;
  outc << "\n};\n";
}

void RamFuzz::gen_mroulette(const string &cls,
                            const unordered_map<string, unsigned> &namecount) {
  unsigned mroulette_size = 0;
  outc << "const harness<" << cls << ">::mptr harness<" << cls
       << ">::mroulette[] = {\n  ";
  const auto name = ctrname(cls);
  bool firstel = true;
  for (const auto &nc : namecount) {
    if (nc.first == name)
      continue; // Skip methods corresponding to constructors under test.
    for (unsigned i = 0; i < nc.second; ++i) {
      if (!firstel)
        outc << ", ";
      firstel = false;
      outc << "&harness<" << cls << ">::" << nc.first << i;
      mroulette_size++;
    }
  }
  outc << "\n};\n";

  outh << "  using mptr = void (harness::*)();\n";
  outh << "  static constexpr unsigned mcount = " << mroulette_size << ";\n";
  outh << "  static const mptr mroulette[mcount];\n";
}

void RamFuzz::gen_int_ctr(const string &cls) {
  outh << "  // Creates obj internally, using indicated constructor.\n";
  outh << "  harness(runtime::gen& g, unsigned ctr);\n";
  outc << "harness<" << cls << ">::harness(runtime::gen& g, unsigned ctr)\n";
  outc << "  : g(g), pobj((this->*croulette[ctr])()), obj(*pobj) {}\n";
  outc << "harness<" << cls << ">::harness(runtime::gen& g)\n";
  outc << "  : g(g), pobj((this->*croulette[g.between(0u,ccount-1)])()), "
          "obj(*pobj) {}\n";
}

void RamFuzz::gen_submakers_decl(const string &cls) {
  outh << "  static const size_t subcount; // How many direct public "
          "subclasses.\n";
  outh << "  // Maker functions for direct public subclasses (subcount "
          "elements).\n";
  outh << "  static " << cls << " *(*const submakers[])(runtime::gen &);\n";
}

void RamFuzz::gen_submakers_defs(const Inheritance &inh) {
  auto next_maker_fn = 0u;
  for (const auto &cls : processed_classes) {
    const auto found = inh.find(cls);
    if (found == inh.end() || found->getValue().empty()) {
      outc << "const size_t harness<" << cls << ">::subcount = 0;\n";
      outc << cls << "*(*const harness<" << cls
           << ">::submakers[])(runtime::gen&) = {};\n";
    } else {
      const auto first_maker_fn = next_maker_fn;
      outc << "namespace {\n";
      for (const auto &subcls : found->getValue())
        outc << cls << "* submakerfn" << next_maker_fn++
             << "(runtime::gen& g) { return g.make<" << subcls.first()
             << ">(true); }\n";
      outc << "} // anonymous namespace\n";
      outc << cls << "*(*const harness<" << cls
           << ">::submakers[])(runtime::gen&) = { ";
      for (auto i = first_maker_fn; i < next_maker_fn; ++i)
        outc << (i == first_maker_fn ? "" : ",") << "submakerfn" << i;
      outc << " };\n";
      outc << "const size_t harness<" << cls
           << ">::subcount = " << next_maker_fn - first_maker_fn << ";\n\n";
    }
  }
}

void RamFuzz::early_exit(const Twine &loc, const Twine &failval,
                         const Twine &reason) {
  outc << "    std::cout << \"" << loc << " exiting early due to " << reason
       << "\" << std::endl;\n";
  outc << "    --calldepth;\n";
  outc << "    return " << failval << ";\n";
}

bool RamFuzz::harness_may_recurse(const CXXMethodDecl *M,
                                  const ASTContext &ctx) {
  for (const auto &ram : M->parameters())
    if (get<0>(ultimate_pointee(ram->getType(), ctx))->isRecordType())
      // Making a class parameter value invokes other RamFuzz code, which may,
      // in turn, invoke M again.  So M's harness may recurse.
      return true;
  return false;
}

void RamFuzz::gen_method(const Twine &hname, const CXXMethodDecl *M,
                         const ASTContext &ctx, bool may_recurse) {
  outc << hname << "() {\n";
  if (may_recurse) {
    outc << "  if (++calldepth >= depthlimit && safectr) {\n";
    early_exit(hname, isa<CXXConstructorDecl>(M) ? "(this->*safectr)()" : "",
               "call depth limit");
    outc << "  }\n";
  }
  if (isa<CXXConstructorDecl>(M)) {
    outc << "  auto r = new ";
    if (M->getParent()->isAbstract())
      outc << "concrete_impl(g" << (M->param_empty() ? "" : ", ");
    else {
      M->getParent()->printQualifiedName(outc);
      outc << "(";
    }
  } else
    outc << "  obj." << *M << "(";
  bool first = true;
  for (const auto &ram : M->parameters()) {
    if (!first)
      outc << ", ";
    first = false;
    QualType valty;
    unsigned ptrcnt;
    tie(valty, ptrcnt) = ultimate_pointee(ram->getType(), ctx);
    if (ptrcnt > 1)
      // Avoid deep const mismatch: can't pass int** for const int** parameter.
      outc << "const_cast<" << ram->getType().stream(prtpol) << ">(";
    outc << "*g.make<"
         << valty.getDesugaredType(ctx).getUnqualifiedType().stream(prtpol);
    ntimes(ptrcnt, "*", outc);
    outc << ">(" << (ptrcnt || ram->getType()->isReferenceType() ? "true" : "")
         << ")";
    if (ptrcnt > 1)
      outc << ")";
    register_enum(*valty);
  }
  outc << ");\n";
  if (may_recurse)
    outc << "  --calldepth;\n";
  if (isa<CXXConstructorDecl>(M))
    outc << "  return r;\n";
  outc << "}\n\n";
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
    if (!globally_visible(C) || C->getDescribedClassTemplate() ||
        isa<ClassTemplateSpecializationDecl>(C))
      return;
    const string cls = C->getQualifiedNameAsString();
    outh << "template<>\n";
    outh << "class harness<" << cls << "> {\n";
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
    outc << "unsigned harness<" << cls << ">::calldepth = 0;\n\n";
    outh << "  static const unsigned depthlimit = "
            "ramfuzz::runtime::depthlimit;\n";
    gen_concrete_impl(C, *Result.Context);
    outh << " public:\n";
    outh << "  using user_class = " << cls << ";\n";
    outh << "  " << cls << "& obj; // Object under test.\n";
    outh << "  harness(runtime::gen& g, " << cls
         << "& obj) : g(g), obj(obj) {} // Object already created by caller.\n";
    outh << "  // True if obj was successfully internally created.\n";
    outh << "  operator bool() const { return bool(pobj); }\n";
    outh << "  // Releases internally generated object and returns its address "
            "\n  // (or null if externally generated).\n";
    outh << "  " << cls << "* release() { return pobj.release(); }\n";
    unordered_map<string, unsigned> namecount;
    bool ctrs = false;
    string safectr;
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
      const bool may_recurse = harness_may_recurse(M, *Result.Context);
      gen_method(Twine("harness<") + cls + ">::" + name +
                     Twine(namecount[name]),
                 M, *Result.Context, may_recurse);
      if (safectr.empty() && !may_recurse && isa<CXXConstructorDecl>(M))
        safectr = name + to_string(namecount[name]);
      namecount[name]++;
    }
    if (C->needsImplicitDefaultConstructor()) {
      const auto name = ctrname(cls);
      safectr = name + to_string(namecount[name]);
      outh << "  " << cls << "* ";
      outh << name << namecount[name]++ << "() { return new ";
      if (C->isAbstract())
        outh << "concrete_impl(g)";
      else
        outh << cls << "()";
      outh << "; }\n";
      ctrs = true;
    }
    gen_mroulette(cls, namecount);
    if (ctrs) {
      gen_int_ctr(cls);
      gen_croulette(cls, namecount[C->getNameAsString()]);
      outh << "  // Ctr safe from depthlimit; won't call another harness "
              "method.\n";
      outh << "  static constexpr cptr safectr = ";
      if (safectr.empty())
        outh << "nullptr";
      else
        outh << "&harness::" << safectr;
      outh << ";\n";
    } else
      outh << "  // No public constructors -- user must provide this:\n";
    outh << "  harness(runtime::gen& g);\n";
    gen_submakers_decl(cls);
    outh << "};\n";
    outc << "\n";
    processed_classes.insert(cls);
  }
}

void RamFuzz::finish(const Inheritance &inh) {
  for (auto e : referenced_enums) {
    outh << "template<> " << e.first << "* ramfuzz::runtime::gen::make<"
         << e.first << ">(bool);\n";
    outc << "template<> " << e.first << "* ramfuzz::runtime::gen::make<"
         << e.first << ">(bool) {\n";
    outc << "  static " << e.first << " a[] = {\n    ";
    int comma = 0;
    for (const auto &n : e.second)
      outc << (comma++ ? "," : "") << n;
    outc << "  };\n";
    outc << "  return &a[between(std::size_t(0), sizeof(a)/sizeof(a[0]) - "
            "1)];\n";
    outc << "}\n";
  }

  gen_submakers_defs(inh);
}

void RamFuzz::tackOnto(MatchFinder &MF) {
  static const auto matcher =
      cxxRecordDecl(isExpansionInMainFile(),
                    unless(hasAncestor(namespaceDecl(isAnonymous()))),
                    hasDescendant(cxxMethodDecl(isPublic())))
          .bind("class");
  MF.addMatcher(matcher, this);
}

namespace ramfuzz {

int genTests(ClangTool &tool, const vector<string> &sources, raw_ostream &outh,
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
  MatchFinder mf;
  RamFuzz rf(outh, outc);
  rf.tackOnto(mf);
  InheritanceBuilder inh;
  inh.tackOnto(mf);
  const int run_error = tool.run(newFrontendActionFactory(&mf).get());
  rf.finish(inh.getInheritance());
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

} // namespace ramfuzz
