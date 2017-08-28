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

#include "Util.hpp"

#include "clang/AST/DeclTemplate.h"
#include "llvm/Support/raw_ostream.h"

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

/// Returns tmpl's parameters formatted as <T1, T2, T3>.  If tmpl is null,
/// returns an empty string.
string parameters(const ClassTemplateDecl *tmpl) {
  string s;
  raw_string_ostream strm(s);
  if (tmpl) {
    strm << '<';
    size_t i = 0;
    for (const auto par : *tmpl->getTemplateParameters())
      strm << (i++ ? ", " : "") << getName(*par, default_typename);
    strm << '>';
  }
  return strm.str();
}

/// Prints \p params to \p os, together with their types.  Eg: "typename T1,
/// class T2, int T3".
void print_names_with_types(const TemplateParameterList &params,
                            raw_ostream &os) {
  /// Similar to DeclPrinter::printTemplateParameters(), but must generate names
  /// for nameless parameters.
  size_t idx = 0;
  for (const auto par : params) {
    os << (idx++ ? ", " : "");
    const auto name = getName(*par, default_typename);
    if (auto type = dyn_cast<TemplateTypeParmDecl>(par))
      os << (type->wasDeclaredWithTypename() ? "typename " : "class ") << name;
    else if (const auto nontype = dyn_cast<NonTypeTemplateParmDecl>(par))
      nontype->getType().print(os, RFPP(), name);
  }
}

/// Returns the preamble "template<...>" required before a template class's
/// name.  If the class isn't a template, or \p templ is null, returns an empty
/// string.
string template_preamble(const clang::ClassTemplateDecl *templ) {
  string stemp;
  raw_string_ostream rs(stemp);
  if (templ) {
    rs << "template<";
    print_names_with_types(*templ->getTemplateParameters(), rs);
    rs << ">\n";
  }
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

StringRef getName(const NamedDecl &decl, const char *deflt) {
  StringRef name;
  if (auto id = decl.getIdentifier())
    name = id->getName();
  return name.empty() ? deflt : name;
}

namespace ramfuzz {

ClassDetails::ClassDetails(const clang::CXXRecordDecl &decl)
    : name_(decl.getQualifiedNameAsString()),
      prefix_(template_preamble(decl.getDescribedClassTemplate())),
      suffix_(parameters(decl.getDescribedClassTemplate())),
      is_template_(decl.getDescribedClassTemplate()),
      is_visible_(globally_visible(&decl)) {}

PrintingPolicy RFPP() {
  PrintingPolicy rfpp((LangOptions()));
  rfpp.Bool = 1;
  rfpp.SuppressUnwrittenScope = true;
  rfpp.SuppressTagKeyword = true;
  rfpp.SuppressScope = false;
  return rfpp;
}

} // namespace ramfuzz
