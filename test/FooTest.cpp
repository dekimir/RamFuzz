#include "ramfuzz/lib/RamFuzz.hpp"
#include "clang/Frontend/FrontendActions.h"
#include "gtest/gtest.h"

namespace {

TEST(Foo, One) { EXPECT_NE("fail", ramfuzz("class X {};")); }

} // anonymous namespace
