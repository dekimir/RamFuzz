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
      strm << (i++ ? ", " : "") << *par;
    strm << '>';
  }
  return strm.str();
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

ClassReference::ClassReference(clang::CXXRecordDecl const &decl)
    : name_(decl.getQualifiedNameAsString()),
      suffix_(parameters(decl.getDescribedClassTemplate())) {}

} // namespace ramfuzz
