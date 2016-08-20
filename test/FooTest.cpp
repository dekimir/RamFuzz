#include "ramfuzz/lib/RamFuzz.hpp"
#include "clang/Frontend/FrontendActions.h"
#include "gtest/gtest.h"

namespace {

using clang::ast_matchers::MatchFinder;
using clang::tooling::newFrontendActionFactory;
using std::ostringstream;

TEST(Foo, One) { EXPECT_NE("fail", ramfuzz("class X {};")); }

} // anonymous namespace
