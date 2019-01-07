// Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
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

#include "Util.hpp"

#include <regex>
#include <utility>

#include "clang/AST/DeclTemplate.h"
#include "llvm/Support/raw_ostream.h"
// Until there's a better way to reuse this code:
#include "clang/../../lib/AST/DeclPrinter.cpp"

using namespace clang;
using namespace llvm;
using namespace ramfuzz;
using namespace std;

namespace {

/// Returns C's or its described template's (if one exists) AccessSpecifier.
AccessSpecifier getAccess(const CXXRecordDecl *C) {
  if (const auto t = C->getDescribedClassTemplate())
    return t->getAccess();
  else
    return C->getAccess();
}

/// Returns template parameters formatted as <T1, T2, T3> if decl is a template
/// or a template specialization.  If not, returns "".
string tparameters(const CXXRecordDecl &decl, NameGetter &ng) {
  string s;
  raw_string_ostream strm(s);
  if (auto tmpl = decl.getDescribedClassTemplate()) {
    strm << '<';
    size_t i = 0;
    for (const auto par : *tmpl->getTemplateParameters())
      strm << (i++ ? ", " : "") << ng.get(par);
    strm << '>';
  } else if (auto spec = dyn_cast<ClassTemplateSpecializationDecl>(&decl)) {
    DeclPrinter(strm, RFPP(), decl.getASTContext())
        .printTemplateArguments(spec->getTemplateInstantiationArgs());
    if (auto partial = dyn_cast<ClassTemplatePartialSpecializationDecl>(spec))
      return sub_canonical_param_types(strm.str(),
                                       *partial->getTemplateParameters());
  }
  return strm.str();
}

/// Prints \p params to \p os, together with their types.  Eg: "typename T1,
/// class T2, int T3".
void print_names_with_types(const TemplateParameterList &params,
                            raw_ostream &os, NameGetter &ng) {
  /// Similar to DeclPrinter::printTemplateParameters(), but must generate names
  /// for nameless parameters.
  size_t idx = 0;
  for (const auto par : params) {
    os << (idx++ ? ", " : "");
    const auto name = ng.get(par);
    if (auto type = dyn_cast<TemplateTypeParmDecl>(par))
      os << (type->wasDeclaredWithTypename() ? "typename " : "class ") << name;
    else if (const auto nontype = dyn_cast<NonTypeTemplateParmDecl>(par))
      nontype->getType().print(os, RFPP(), name);
  }
}

/// Returns the preamble "template<...>" required before a template class's
/// name.  If the class isn't a template, or \p templ is null, returns an empty
/// string.
string template_preamble(const clang::ClassTemplateDecl *templ,
                         NameGetter &ng) {
  string stemp;
  raw_string_ostream rs(stemp);
  if (templ) {
    rs << "template<";
    print_names_with_types(*templ->getTemplateParameters(), rs, ng);
    rs << ">\n";
  }
  return rs.str();
}

/// Invokes fun on a DeclPrinter set up over a string stream.  Returns the
/// stream's string after the invocation.
template <typename Callable>
string WithDeclPrinter(const ASTContext &ctx, const Callable &fun) {
  string temp;
  raw_string_ostream rs(temp);
  DeclPrinter p(rs, RFPP(), ctx);
  fun(p);
  return rs.str();
}

} // anonymous namespace

bool globally_visible(const CXXRecordDecl *C) {
  if (!C || !C->getIdentifier())
    // Anonymous classes may technically be visible, but only through tricks
    // like decltype.  Skip until there's a compelling use-case.
    return false;
  const auto acc = getAccess(C);
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

namespace ramfuzz {

string sub_canonical_param_types(string s,
                                 const TemplateParameterList &params) {
  const auto ppol = RFPP();
  for (const auto ram : params)
    if (const auto pd = dyn_cast<TemplateTypeParmDecl>(ram)) {
      const auto ty = cast<TemplateTypeParmType>(pd->getTypeForDecl());
      const auto real = pd->getNameAsString();
      const auto canon = ty->getCanonicalTypeInternal().getAsString(ppol);
      s = regex_replace(s, regex(canon), real);
    }
  return s;
}

ClassDetails::ClassDetails(const CXXRecordDecl &decl, NameGetter &ng)
    : name_(decl.getNameAsString()), qname_(decl.getQualifiedNameAsString()),
      prefix_(template_preamble(decl.getDescribedClassTemplate(), ng)),
      suffix_(tparameters(decl, ng)),
      is_template_(isa<ClassTemplateSpecializationDecl>(decl) ||
                   decl.getDescribedClassTemplate()),
      is_visible_(globally_visible(&decl)) {
  if (const auto partial =
          dyn_cast<ClassTemplatePartialSpecializationDecl>(&decl)) {
    prefix_ = WithDeclPrinter(decl.getASTContext(), [partial](DeclPrinter &p) {
      p.printTemplateParameters(partial->getTemplateParameters());
    });
    const auto sub = sub_canonical_param_types(
        WithDeclPrinter(decl.getASTContext(),
                        [partial](DeclPrinter &p) {
                          p.printTemplateArguments(
                              partial->getTemplateArgs(),
                              partial->getTemplateParameters());
                        }),
        *partial->getTemplateParameters());
  } else if (const auto spec =
                 dyn_cast<ClassTemplateSpecializationDecl>(&decl)) {
    const auto args =
        WithDeclPrinter(decl.getASTContext(), [spec](DeclPrinter &p) {
          p.printTemplateArguments(spec->getTemplateInstantiationArgs());
        });
  }
}

PrintingPolicy RFPP() {
  PrintingPolicy rfpp((LangOptions()));
  rfpp.Bool = 1;
  rfpp.SuppressUnwrittenScope = true;
  rfpp.SuppressTagKeyword = true;
  rfpp.SuppressScope = false;
  return rfpp;
}

StringRef NameGetter::get(const NamedDecl *decl) {
  StringRef name;
  if (auto id = decl->getIdentifier())
    name = id->getName();
  if (!name.empty())
    return name;
  const auto found = placeholders.find(decl);
  if (found != placeholders.end())
    return found->second;
  return placeholders
      .insert(make_pair(decl, placeholder_prefix + to_string(watermark++)))
      .first->second;
}

} // namespace ramfuzz
