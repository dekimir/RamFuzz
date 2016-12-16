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

#include "Inheritance.hpp"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

namespace {

using namespace std;

using namespace clang;
using namespace ast_matchers;
using namespace tooling;

using ramfuzz::Inheritance;

auto ClassMatcher =
    cxxRecordDecl(unless(hasAncestor(namespaceDecl(isAnonymous()))),
                  unless(isImplicit()))
        .bind("class");

/// Builds up an Inheritance object from MatchFinder matches, which must bind a
/// CXXRecordDecl* to "class".
///
/// The user can feed an InheritanceBuilder instance to a custom MatchFinder, or
/// simply getActionFactory() and run it in a ClangTool (this will process all
/// classes outside anonymous namespaces).
class InheritanceBuilder : public MatchFinder::MatchCallback {
public:
  InheritanceBuilder() {
    MF.addMatcher(ClassMatcher, this);
    AF = newFrontendActionFactory(&MF);
  }

  /// Match callback.  Expects Result to have a binding for "class".
  void run(const MatchFinder::MatchResult &Result) override {
    if (const auto *C = Result.Nodes.getNodeAs<CXXRecordDecl>("class"))
      for (const auto &base : C->bases())
        if (base.getAccessSpecifier() == AS_public) {
          const auto decl = dyn_cast<CXXRecordDecl>(base.getType()
                                                        ->castAs<RecordType>()
                                                        ->getDecl()
                                                        ->getCanonicalDecl());
          inh[decl].insert(C);
        }
  }

  FrontendActionFactory &getActionFactory() { return *AF; }

  const Inheritance &getInheritance() { return inh; }

private:
  /// A FrontendActionFactory to run MF.  Owned by *this because it
  /// requires live MF to remain valid.
  unique_ptr<FrontendActionFactory> AF;

  /// A MatchFinder to run *this on ClassMatcher.  Owned by *this
  /// because it's only valid while *this is alive.
  MatchFinder MF;

  /// Result being built.
  Inheritance inh;
};

} // anonymous namespace

namespace ramfuzz {
Inheritance findInheritance(const llvm::Twine &code) {
  InheritanceBuilder tracker;
  clang::tooling::runToolOnCode(tracker.getActionFactory().create(), code);
  return tracker.getInheritance();
}
} // namespace ramfuzz
