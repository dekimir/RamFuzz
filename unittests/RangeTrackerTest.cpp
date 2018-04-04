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

#include "gtest/gtest.h"

#include "ramfuzz/lib/RangeTracker.hpp"

#include <limits>
#include <utility>

namespace {

using namespace ramfuzz;
using namespace std;
using namespace testing;

/// A helper representing a variable in LinearCombination.
struct x {
  size_t id;
};

/// A helper to build LinearCombinations via math-like notation, eg, 1.2*x{34}.
LinearCombination operator*(double m, x x_) {
  return LinearCombination{{make_pair(x_.id, m)}, 0};
}

/// A helper to add offset via math-like notation, eg, x{1} >= 150.
LinearCombination operator>=(LinearCombination c, double lb) {
  auto result = c;
  result.offset -= lb;
  return result;
}

constexpr double maxdbl = numeric_limits<double>::max(),
                 mindbl = numeric_limits<double>::min();

TEST(RangeTrackerTest, Empty) {
  EXPECT_EQ(make_pair(mindbl, maxdbl), bounds(1234, {}));
}

TEST(RangeTrackerTest, Single) {
  EXPECT_EQ(make_pair(2., maxdbl),
            bounds(1, {LinearInequality{3. * x{1} >= 6.}}));
}

TEST(RangeTrackerTest, Unconstrained) {
  LinearInequality li{1. * x{1} + 1. * x{2} >= 0.};
  EXPECT_EQ(make_pair(mindbl, maxdbl), bounds(1, {li}));
  EXPECT_EQ(make_pair(mindbl, maxdbl), bounds(2, {li}));
}

TEST(RangeTrackerTest, UpperAndLower) {
  EXPECT_EQ(make_pair(1000., 2000.),
            bounds(1, {LinearInequality{1. * x{1} >= 1000.},
                       LinearInequality{-1. * x{1} >= -2000.}}));
}

TEST(RangeTrackerTest, Chain) {
  EXPECT_EQ(make_pair(125., maxdbl),
            bounds(1, {LinearInequality{1. * x{2} >= 123.},
                       LinearInequality{1. * x{1} - 1. * x{2} >= 2.}}));
}

} // anonymous namespace
