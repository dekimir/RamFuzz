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

#include "Inheritance.hpp"

#include "Util.hpp"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

using namespace std;

using namespace clang;
using namespace ast_matchers;
using namespace tooling;

namespace ramfuzz {

void InheritanceBuilder::tackOnto(MatchFinder &MF) {
  MF.addMatcher(cxxRecordDecl(isDefinition(),
                              unless(hasAncestor(namespaceDecl(isAnonymous()))))
                    .bind("class"),
                this);
}

void InheritanceBuilder::run(const MatchFinder::MatchResult &Result) {
  if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class"))
    for (const auto &base : C->bases()) {
      const auto qname = C->getQualifiedNameAsString();
      if (base.getAccessSpecifier() == AS_public) {
        PrintingPolicy prtpol((LangOptions()));
        prtpol.SuppressTagKeyword = true;
        prtpol.SuppressScope = false;
        if (const auto tag = base.getType()
                                 .getDesugaredType(*Result.Context)
                                 ->getAs<TagType>())
          inh[ClassDetails(*dyn_cast<CXXRecordDecl>(tag->getDecl()),
                           tparam_names)]
              .emplace_back(*C, tparam_names);
      }
    }
}

void InheritanceBuilder::process(const llvm::Twine &Code) {
  MatchFinder MF;
  tackOnto(MF);
  runToolOnCode(newFrontendActionFactory(&MF)->create(), Code);
}

} // namespace ramfuzz
