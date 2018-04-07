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

} // anonymous namespace

bool check_subst() {
  LinearInequality li{1. * x{1} + 2. * x{2} >= 3.};
  li.substitute(1, 3.);
  return LinearInequality{2. * x{2} >= 0.} == li;
}

int main(int argc, char *argv[]) {
  if (!check_subst())
    return 1;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
