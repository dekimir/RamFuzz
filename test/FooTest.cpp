#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

namespace {

TEST(Foo, One) {
  EXPECT_TRUE(clang::tooling::runToolOnCode(new clang::SyntaxOnlyAction,
                                            "class X {};"));
}

} // anonymous namespace
