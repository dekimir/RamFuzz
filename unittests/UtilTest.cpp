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

#include "gtest/gtest.h"

#include "ramfuzz/lib/Util.hpp"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

namespace {

using namespace llvm;
using namespace ramfuzz;
using namespace std;
using namespace testing;

using namespace clang;
using namespace ast_matchers;
using namespace tooling;

class Helper : public MatchFinder::MatchCallback {
  ClassDetails obj;

public:
  static ClassDetails process(const Twine &code) {
    Helper h;
    MatchFinder MF;
    MF.addMatcher(cxxRecordDecl(unless(isImplicit())).bind("class"), &h);
    runToolOnCode(newFrontendActionFactory(&MF)->create(), code);
    return h.obj;
  }

  void run(const MatchFinder::MatchResult &result) {
    if (const auto *C = result.Nodes.getNodeAs<CXXRecordDecl>("class"))
      obj = ClassDetails(*C);
  }
};

TEST(ClassDetailsTest, Bare) {
  EXPECT_EQ("C", Helper::process("class C {};").qname());
}

TEST(ClassDetailsTest, Qualified) {
  EXPECT_EQ("N::C", Helper::process("namespace N {class C {};}").qname());
}

} // anonymous namespace
