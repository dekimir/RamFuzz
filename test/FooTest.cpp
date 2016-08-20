#include <string>

#include "ramfuzz/lib/RamFuzz.hpp"
#include "clang/Frontend/FrontendActions.h"
#include "gtest/gtest.h"

namespace {

using clang::SyntaxOnlyAction;
using clang::tooling::runToolOnCode;
using std::string;

/// True if code has valid syntax.
bool oksyntax(const string &code) {
  return runToolOnCode(new SyntaxOnlyAction(), code);
}

TEST(OksyntaxTest, ValidCode) { EXPECT_TRUE(oksyntax("int a = 123;")); }
TEST(OksyntaxTest, InvalidCode) { EXPECT_FALSE(oksyntax("int a =;")); }
TEST(OksyntaxTest, FailLiteral) { EXPECT_FALSE(oksyntax("fail")); }

TEST(RamFuzzTest, EmptyClass) { EXPECT_EQ("", ramfuzz("class X {};")); }

} // anonymous namespace
