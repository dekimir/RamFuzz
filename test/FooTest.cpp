#include <sstream>

#include "ramfuzz/lib/RamFuzz.hpp"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

namespace {

using clang::ast_matchers::MatchFinder;
using clang::tooling::newFrontendActionFactory;
using std::ostringstream;

TEST(Foo, One) {
  ostringstream str;
  RamFuzz RF(str);
  MatchFinder MF = RF.makeMatchFinder();
  EXPECT_TRUE(clang::tooling::runToolOnCode(
      newFrontendActionFactory(&MF)->create(), "class X {};"));
}

} // anonymous namespace
