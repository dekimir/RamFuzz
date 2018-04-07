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

#include <limits>
#include <utility>

#include "ramfuzz-rt.hpp"

using namespace std;
using namespace ramfuzz::runtime;

namespace {

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

} // anonymous namespace

bool check_subst() {
  LinearInequality li{1. * x{1} + 2. * x{2} >= 3.};
  li.substitute(1, 3.);
  return LinearInequality{2. * x{2} >= 0.} == li;
}

bool check_bounds_empty() {
  return make_tuple(mindbl, maxdbl) == bounds(1234, {});
}

bool check_bounds_single() {
  return make_tuple(2., maxdbl) ==
         bounds(1, {LinearInequality{3. * x{1} >= 6.}});
}

bool check_bounds_unconstrained() {
  LinearInequality li{1. * x{1} + 1. * x{2} >= 0.};
  return make_tuple(mindbl, maxdbl) == bounds(1, {li}) &&
         make_tuple(mindbl, maxdbl) == bounds(2, {li});
}

bool check_bounds_upperandlower() {
  return make_tuple(1000., 2000.) ==
         bounds(1, {LinearInequality{1. * x{1} >= 1000.},
                    LinearInequality{-1. * x{1} >= -2000.}});
}

bool check_bounds_chain() {
  return make_tuple(125., maxdbl) ==
         bounds(1, {LinearInequality{1. * x{2} >= 123.},
                    LinearInequality{1. * x{1} - 1. * x{2} >= 2.}});
}

bool check_bounds_zeromultiplier() {
  return make_tuple(123., maxdbl) ==
         bounds(2, {LinearInequality{0. * x{1} + 1. * x{2} >= 123.},
                    LinearInequality{1. * x{1} - 1. * x{2} >= 0.}});
}

int main(int argc, char *argv[]) {
  if (!check_subst())
    return 1;
  if (!check_bounds_empty())
    return 2;
  if (!check_bounds_single())
    return 3;
  if (!check_bounds_unconstrained())
    return 4;
  if (!check_bounds_upperandlower())
    return 5;
  if (!check_bounds_chain())
    return 6;
  if (!check_bounds_zeromultiplier())
    return 7;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
