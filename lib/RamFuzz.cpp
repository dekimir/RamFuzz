// Copyright 2016-2017 The RamFuzz contributors. All rights reserved.
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
#include "Util.hpp"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

using namespace ramfuzz;
using namespace std;

using namespace clang;
using namespace ast_matchers;
using namespace tooling;

using llvm::raw_ostream;
using llvm::raw_string_ostream;

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

/// An object that can be streamed to raw_ostream.  Subclassed for concrete
/// content.
class streamable {
public:
  /// Streams content to \p os.
  virtual void print(raw_ostream &os) const = 0;

  virtual ~streamable() = default;
};

/// Streams contents of \p thiz to \p os.
raw_ostream &operator<<(raw_ostream &os, const streamable &thiz) {
  thiz.print(os);
  return os;
}

/// A streaming adapter for QualType.  Prints C++ code that compiles correctly
/// in the RamFuzz context (unlike Clang's TypePrinter:
/// https://github.com/dekimir/RamFuzz/issues/1).
class type_streamer : public streamable {
public:
  /// Prepares to stream the given type.
  type_streamer(const QualType &ty, const PrintingPolicy &prtpol)
      : ty(ty), prtpol(prtpol) {}

  /// Prints the constructor's argument \p ty out to \p os.
  void print(raw_ostream &os) const override {
    if (auto el = ty->getAs<ElaboratedType>()) {
      print_cv(os);
      os << type_streamer(el->desugar(), prtpol);
    } else if (auto spec = ty->getAs<TemplateSpecializationType>()) {
      print_cv(os);
      print(os, spec->getTemplateName());
      size_t idx = 0;
      for (auto arg : spec->template_arguments()) {
        os << (idx++ ? ", " : "< "); // Space after < avoids <:.
        if (arg.getKind() == TemplateArgument::Type)
          os << type_streamer(arg.getAsType(), prtpol);
        else
          arg.print(prtpol, os);
      }
      os << '>';
    } else if (auto td = ty->getAs<TypedefType>()) {
      td->getDecl()->printQualifiedName(os, prtpol);
    } else if (ty->isReferenceType()) {
      os << type_streamer(ty.getNonReferenceType(), prtpol) << '&';
      // TODO: handle lvalue references.
    } else if (ty->isPointerType()) {
      const auto ptee = ty->getPointeeType();
      if (const auto funty = ptee->getAs<FunctionProtoType>()) {
        os << type_streamer(funty->getReturnType(), prtpol) << "(*)";
        print(os, funty->getParamTypes());
      } else
        os << type_streamer(ptee, prtpol) << '*';
    } else if (auto inj = ty->getAs<InjectedClassNameType>()) {
      os << type_streamer(inj->getInjectedSpecializationType(), prtpol);
    } else if (auto dep = ty->getAs<DependentNameType>()) {
      os << "typename ";
      print(os, *dep->getQualifier());
      os << dep->getIdentifier()->getName();
    } else
      ty.print(os, prtpol);
    // TODO: make this fully equivalent to TypePrinter, handling all possible
    // types or cleverly deferring to it.
  }

  /// Prints "const " and/or "volatile " to \p os if appropriate.
  void print_cv(raw_ostream &os) const {
    if (ty.isLocalConstQualified())
      os << "const ";
    if (ty.isLocalVolatileQualified())
      os << "volatile ";
  }

  /// Prints \p name to \p os.
  void print(raw_ostream &os, const TemplateName &name) const {
    if (auto decl = name.getAsTemplateDecl())
      decl->printQualifiedName(os, prtpol);
    else
      name.print(os, prtpol);
  }

  /// Prints \p qual to \p os.
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
      os << type_streamer(QualType(qual.getAsType(), 0), prtpol);
      break;
    default:
      break;
    }
    os << "::";
  }

  /// Prints all elements of \p typelist to \p os, surrounded by parentheses and
  /// separated by commas.
  void print(raw_ostream &os, const ArrayRef<QualType> typelist) const {
    os << '(';
    for (auto cur = typelist.begin(); cur != typelist.end(); ++cur) {
      if (cur != typelist.begin())
        os << ", ";
      os << type_streamer(*cur, prtpol);
    }
    os << ')';
  }

private:
  const QualType &ty;
  const PrintingPolicy &prtpol;
};

/// Streams a method's name.
class method_streamer : public streamable {
public:
  method_streamer(const CXXMethodDecl &m, const PrintingPolicy &prtpol)
      : m(m), prtpol(prtpol) {}
  void print(raw_ostream &os) const override {
    if (const auto con = dyn_cast<CXXConversionDecl>(&m))
      // Stream the conversion type's name correctly.
      os << "operator " << type_streamer(con->getConversionType(), prtpol);
    else
      os << m;
  }

private:
  const CXXMethodDecl &m;
  const PrintingPolicy &prtpol;
};

/// Generates RamFuzz code into an ostream.  The user can tack a RamFuzz
/// instance onto a MatchFinder for running it via a frontend action.  After the
/// frontend action completes, the user must call finish().
class RamFuzz : public MatchFinder::MatchCallback {
public:
  /// Prepares for emitting RamFuzz code into outh and outc.
  RamFuzz(raw_ostream &outh, raw_ostream &outc)
      : outh(outh), outc(outc), prtpol(RFPP()), tparam_names(default_typename) {
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

  /// If ty is a class, adds it to referenced_classes.
  void register_class(const Type &ty);

  void reg(const Type &ty) {
    register_enum(ty);
    register_class(ty);
  }

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
  void gen_croulette(const ClassDetails &cls, ///< Class under test.
                     unsigned size            ///< Size of croulette.
  );

  /// Generates the declaration and definition of member mroulette.
  void gen_mroulette(
      const ClassDetails &cls, ///< Class under test.
      const unordered_map<string, unsigned>
          &namecount ///< Method-name histogram of the class under test.
  );

  /// Generates the declaration of cls member submakers.
  void gen_submakers_decl(const ClassDetails &cls);

  /// Generates the definition of member submakers for each of the classes
  /// processed so far.
  void gen_submakers_defs(const Inheritance &);

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
  /// the method under test M.  Assumes that the return type and scope of the
  /// generated method have already been output.
  void gen_method(
      const Twine &hname, ///< Harness method name.
      const CXXMethodDecl *M, const ASTContext &ctx,
      bool may_recurse ///< True iff generated body may recursively call itself.
  );

  /// Where to output generated declarations (typically a header file).
  raw_ostream &outh;

  /// Where to output generated code (typically a C++ source file).
  raw_ostream &outc;

  /// Where to output generated definitions of possibly templated code.
  unique_ptr<raw_string_ostream> outt;

  /// Policy for printing to outh and outc.
  PrintingPolicy prtpol;

  /// Classes under test that were referenced in generated code.
  set<ClassDetails> referenced_classes;

  /// Qualified names of classes under test whose harness specializations have
  /// been generated.
  set<ClassDetails> processed_classes;

  /// Enum types for which parameters have been generated.  Maps the enum name
  /// to its values.
  unordered_map<string, vector<string>> referenced_enums;

  NameGetter tparam_names; ///< Gets template-parameter names.
};

/// Valid identifier from a CXXMethodDecl name.
string valident(const string &mname) {
  static const unordered_map<char, char> table = {
      {' ', '_'}, {'=', 'e'}, {'+', 'p'}, {'-', 'm'}, {'*', 's'},
      {'/', 'd'}, {'%', 'c'}, {'&', 'a'}, {'|', 'f'}, {'^', 'r'},
      {'<', 'l'}, {'>', 'g'}, {'~', 't'}, {'!', 'b'}, {'[', 'h'},
      {']', 'i'}, {'(', 'j'}, {')', 'k'}, {'.', 'n'}, {',', 'v'},
  };
  string transf = mname;
  for (char &c : transf) {
    auto found = table.find(c);
    if (found != table.end())
      c = found->second;
  }
  return transf;
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

/// Returns C's qualified name, followed by C's template parameters if C is a
/// template class.  It's equivalent to constructing ClassDetails(*C) and
/// concatenating its qname() and suffix().
string class_under_test(const CXXRecordDecl *C, NameGetter& ng) {
  string name = C->getQualifiedNameAsString();
  raw_string_ostream strm(name);
  if (const auto tmpl = C->getDescribedClassTemplate()) {
    strm << '<';
    size_t i = 0;
    for (const auto par : *tmpl->getTemplateParameters())
      strm << (i++ ? ", " : "") << ng.get(par);
    strm << '>';
  }
  return strm.str();
}

} // anonymous namespace

vector<string> RamFuzz::missingClasses() {
  vector<ClassDetails> diff;
  set_difference(referenced_classes.cbegin(), referenced_classes.cend(),
                 processed_classes.cbegin(), processed_classes.cend(),
                 inserter(diff, diff.begin()));
  vector<string> names;
  for (const auto &d : diff)
    names.push_back(d.qname());
  return names;
}

void RamFuzz::register_enum(const Type &ty) {
  if (const auto et = ty.getAs<EnumType>()) {
    const auto decl = et->getDecl();
    const string name = decl->getQualifiedNameAsString();
    for (const auto c : decl->enumerators())
      referenced_enums[name].push_back(c->getQualifiedNameAsString());
  }
}

void RamFuzz::register_class(const Type &ty) {
  auto rec = ty.getAsCXXRecordDecl();
  if (!rec || rec->isInStdNamespace())
    return;
  if (const auto t = dyn_cast<ClassTemplateSpecializationDecl>(rec))
    rec = t->getSpecializedTemplate()->getTemplatedDecl();
  referenced_classes.insert(ClassDetails(*rec, tparam_names));
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
      auto Mrty = type_streamer(M->getReturnType(), prtpol);
      outh << "    " << Mrty << " " << *M << "(";
      outc << Mrty << " harness<" << cls << ">::concrete_impl::" << *M << "(";
      for (auto P = bg; P != en; ++P) {
        auto Pty = type_streamer((*P)->getType(), prtpol);
        outh << mcom(P) << Pty;
        outc << mcom(P) << Pty;
      }
      outh << ") " << (M->isConst() ? "const " : "") << "override;\n";
      outc << ") " << (M->isConst() ? "const " : "") << "{\n";
      auto rety =
          M->getReturnType().getDesugaredType(ctx).getLocalUnqualifiedType();
      if (!rety->isVoidType()) {
        outc << "  return *ramfuzzgenuniquename.make<"
             << type_streamer(rety.getNonReferenceType().getUnqualifiedType(),
                              prtpol)
             << ">("
             << (rety->isPointerType() || rety->isReferenceType() ? "true" : "")
             << ");\n";
        reg(*get<0>(ultimate_pointee(rety, ctx)));
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
          outh << ", " << type_streamer((*P)->getType(), prtpol) << " p"
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

void RamFuzz::gen_croulette(const ClassDetails &cls, unsigned size) {
  outh << "  using cptr = " << cls << "* (harness::*)();\n";
  outh << "  static constexpr unsigned ccount = " << size << ";\n";
  outh << "  static const cptr croulette[ccount];\n";

  *outt << cls.tpreamble() << "const typename harness<" << cls
        << ">::cptr harness<" << cls << ">::croulette[] = {\n  ";
  for (unsigned i = 0; i < size; ++i)
    *outt << (i ? ", " : "") << "&harness<" << cls
          << ">::" << valident(cls.name()) << i;
  *outt << "\n};\n";
}

void RamFuzz::gen_mroulette(const ClassDetails &cls,
                            const unordered_map<string, unsigned> &namecount) {
  unsigned mroulette_size = 0;
  *outt << cls.tpreamble() << "const typename harness<" << cls
        << ">::mptr harness<" << cls << ">::mroulette[] = {\n  ";
  const auto name = valident(cls.name());
  size_t idx = 0;
  for (const auto &nc : namecount) {
    if (nc.first == name)
      continue; // Skip methods corresponding to constructors under test.
    for (unsigned i = 0; i < nc.second; ++i) {
      *outt << (idx++ ? ", " : "") << "&harness<" << cls << ">::" << nc.first
            << i;
      mroulette_size++;
    }
  }
  *outt << "\n};\n";

  outh << "  using mptr = void (harness::*)();\n";
  outh << "  static constexpr unsigned mcount = " << mroulette_size << ";\n";
  outh << "  static const mptr mroulette[mcount];\n";
}

void RamFuzz::gen_submakers_decl(const ClassDetails &cls) {
  outh << "  static const size_t subcount; // How many direct public "
          "subclasses.\n";
  outh << "  // Maker functions for direct public subclasses (subcount "
          "elements).\n";
  outh << "  static " << cls << " *(*const submakers[])(runtime::gen &);\n";
}

void RamFuzz::gen_submakers_defs(const Inheritance &sc) {
  auto next_maker_fn = 0u;
  for (const auto &cls : processed_classes) {
    const auto name = cls.qname() + cls.tparams();
    const auto &tmpl_preamble = cls.tpreamble();
    string stemp;
    outt.reset(new raw_string_ostream(stemp));
    const auto found = sc.find(cls);
    if (found == sc.end() || found->second.empty()) {
      *outt << tmpl_preamble << "const size_t harness<" << name
            << ">::subcount = 0;\n";
      *outt << tmpl_preamble << name << "*(*const harness<" << name
            << ">::submakers[])(runtime::gen&) = {};\n";
    } else {
      const auto first_maker_fn = next_maker_fn;
      *outt << "namespace {\n";
      for (const auto &subcls : found->second)
        if (!subcls.is_template() && subcls.is_visible()) {
          *outt << tmpl_preamble << name << "* submakerfn" << next_maker_fn++
                << "(runtime::gen& g) { return g.make<" << subcls.qname()
                << ">(true); }\n";
          referenced_classes.insert(subcls);
        }
      *outt << "} // anonymous namespace\n";
      *outt << name << "*(*const harness<" << name
            << ">::submakers[])(runtime::gen&) = { ";
      for (auto i = first_maker_fn; i < next_maker_fn; ++i)
        *outt << (i == first_maker_fn ? "" : ",") << "submakerfn" << i;
      *outt << " };\n";
      *outt << tmpl_preamble << "const size_t harness<" << name
            << ">::subcount = " << next_maker_fn - first_maker_fn << ";\n\n";
    }
    (tmpl_preamble.empty() ? outc : outh) << outt->str();
  }
}

bool RamFuzz::harness_may_recurse(const CXXMethodDecl *M,
                                  const ASTContext &ctx) {
  for (const auto &ram : M->parameters()) {
    const auto t = get<0>(ultimate_pointee(ram->getType(), ctx));
    if (t->isRecordType() || isa<InjectedClassNameType>(t))
      // Making a class parameter value invokes other RamFuzz code, which may,
      // in turn, invoke M again.  So M's harness may recurse.
      return true;
  }
  return false;
}

void RamFuzz::gen_method(const Twine &hname, const CXXMethodDecl *M,
                         const ASTContext &ctx, bool may_recurse) {
  *outt << hname << "() {\n";
  if (isa<CXXConstructorDecl>(M)) {
    if (may_recurse) {
      *outt << "  if (++calldepth >= depthlimit && safectr) {\n";
      *outt << "    --calldepth;\n";
      *outt << "    return (this->*safectr)();\n";
      *outt << "  }\n";
    }
    const auto parent = M->getParent();
    *outt << "  auto r = new ";
    if (parent->isAbstract())
      *outt << "concrete_impl(g" << (M->param_empty() ? "" : ", ");
    else
      *outt << class_under_test(parent, tparam_names) << "(";
  } else {
    if (may_recurse) {
      *outt << "  if (++calldepth >= depthlimit) {\n";
      *outt << "    --calldepth;\n";
      *outt << "    return;\n";
      *outt << "  }\n";
    }
    *outt << "  obj->" << method_streamer(*M, prtpol) << "(";
  }
  size_t idx = 0;
  for (const auto &ram : M->parameters()) {
    *outt << (idx++ ? ", " : "");
    QualType valty;
    unsigned ptrcnt;
    tie(valty, ptrcnt) = ultimate_pointee(ram->getType(), ctx);
    if (ptrcnt > 1)
      // Avoid deep const mismatch: can't pass int** for const int** parameter.
      *outt << "const_cast<" << ram->getType().stream(prtpol) << ">(";
    const bool is_rvalue_ref = ram->getType()->isRValueReferenceType();
    if (is_rvalue_ref)
      // This will leave a stored object in an unspecified (though not illegal)
      // state.  It should be possible to subsequently call some of its methods
      // -- eg, this is legal:
      //
      // std::string s("abc");
      // std::string r = std::move(s);
      // s.clear();
      *outt << "std::move(";
    const auto strty = ram->getType()
                           .getDesugaredType(ctx)
                           .getNonReferenceType()
                           .getUnqualifiedType();
    *outt << "*g.make<" << type_streamer(strty, prtpol);
    reg(*strty);
    *outt << ">(" << (ptrcnt || ram->getType()->isReferenceType() ? "true" : "")
          << ")";
    if (is_rvalue_ref)
      *outt << ")";
    if (ptrcnt > 1)
      *outt << ")";
    register_enum(*valty);
  }
  *outt << ");\n";
  if (may_recurse)
    *outt << "  --calldepth;\n";
  if (isa<CXXConstructorDecl>(M))
    *outt << "  return r;\n";
  *outt << "}\n\n";
}

void RamFuzz::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
    if (!globally_visible(C) || isa<ClassTemplateSpecializationDecl>(C))
      return;
    string stemp;
    outt.reset(new raw_string_ostream(stemp));
    ClassDetails cls(*C, tparam_names);
    const auto tmpl = C->getDescribedClassTemplate();
    outh << cls.tpreamble();
    if (cls.tpreamble().empty())
      outh << "template<>";
    outh << "\n";
    outh << "class harness<" << cls << "> {\n";
    outh << " private:\n";
    outh << "  runtime::gen& g; // Declare first to initialize early; "
            "constructors may use it.\n";
    // Call depth should be made atomic when we start supporting multi-threaded
    // fuzzing.  Holding off for now because we expect to get a lot of mileage
    // out of multi-process fuzzing (running multiple fuzzing executables, each
    // in its own process).  That should still keep all the hardware occupied
    // without paying for the overhead of thread-safety.
    outh << "  // Prevents infinite recursion.\n";
    outh << "  static unsigned calldepth;\n";
    *outt << cls.tpreamble() << "unsigned harness<" << cls
          << ">::calldepth = 0;\n\n";
    outh << "  static const unsigned depthlimit = "
            "ramfuzz::runtime::depthlimit;\n";
    gen_concrete_impl(C, *Result.Context);
    outh << " public:\n";
    outh << "  using user_class = " << cls << ";\n";
    outh << "  " << cls << "* obj; // Object under test.\n";
    outh << "  // True if obj was successfully internally created.\n";
    outh << "  operator bool() const { return obj; }\n";
    unordered_map<string, unsigned> namecount;
    size_t ccount = 0;
    string safectr;
    for (auto M : C->methods()) {
      if (isa<CXXDestructorDecl>(M) || M->getAccess() != AS_public ||
          !M->isInstance() || M->isDeleted())
        continue;
      const string name =
          // M->getNameAsString() sometimes uses wrong template-parameter names;
          // see ramfuzz/test/tmpl.hpp.
          valident(isa<CXXConstructorDecl>(M) ? cls.name()
                                              : M->getNameAsString());
      *outt << cls.tpreamble();
      if (isa<CXXConstructorDecl>(M)) {
        outh << "  " << cls << "* ";
        *outt << cls << "* ";
        ccount++;
      } else {
        outh << "  void ";
        *outt << "void ";
      }
      outh << name << namecount[name] << "();\n";
      const bool may_recurse = harness_may_recurse(M, *Result.Context);
      *outt << "harness<" << cls << ">::";
      gen_method(Twine(name) + Twine(namecount[name]), M, *Result.Context,
                 may_recurse);
      if (safectr.empty() && !may_recurse && isa<CXXConstructorDecl>(M))
        safectr = name + to_string(namecount[name]);
      namecount[name]++;
    }
    if (C->needsImplicitDefaultConstructor()) {
      const auto name = valident(cls.name());
      safectr = name + to_string(namecount[name]);
      outh << "  " << cls << "* ";
      outh << name << namecount[name]++ << "() { return new ";
      if (C->isAbstract())
        outh << "concrete_impl(g)";
      else
        outh << cls << "()";
      outh << "; }\n";
      ccount++;
    }
    for (const auto f : C->fields()) {
      const auto ty = f->getType();
      if (f->getAccess() == AS_public && !ty.isConstQualified() &&
          !ty->getAsCXXRecordDecl()) {
        const Twine name = Twine("random_") + f->getName();
        outh << "  void " << name << namecount[name.str()] << "();\n";
        *outt << cls.tpreamble() << "void harness<" << cls << ">::" << name
              << namecount[name.str()] << "() {\n";
        *outt << "  obj->" << *f << " = *g.make<" << type_streamer(ty, prtpol)
              << ">();\n";
        reg(*ty);
        *outt << "}\n";
        namecount[name.str()]++;
      }
    }
    gen_mroulette(cls, namecount);
    if (ccount) {
      gen_croulette(cls, ccount);
      outh << "  // Ctr safe from depthlimit; won't call another harness "
              "method.\n";
      outh << "  static constexpr cptr safectr = ";
      if (safectr.empty())
        outh << "nullptr";
      else
        outh << "&harness::" << safectr;
      outh << ";\n";
      *outt
          << cls.tpreamble() << "harness<" << cls
          << ">::harness(runtime::gen& g)\n"
          << "  : g(g), obj((this->*croulette[g.between(0u,ccount-1)])()) {}\n";
    } else
      outh << "  // No public constructors -- user must provide this:\n";
    outh << "  harness(runtime::gen& g);\n";
    gen_submakers_decl(cls);
    outh << "};\n";
    *outt << "\n";
    (tmpl ? outh : outc) << outt->str();
    processed_classes.insert(cls);
  }
}

void RamFuzz::finish(const Inheritance &sc) {
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

  gen_submakers_defs(sc);
}

void RamFuzz::tackOnto(MatchFinder &MF) {
  static const auto matcher =
      cxxRecordDecl(isExpansionInMainFile(), isDefinition(),
                    unless(hasAncestor(namespaceDecl(isAnonymous()))),
                    unless(isImplicit()))
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
