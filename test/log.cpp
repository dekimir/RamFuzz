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

#include <memory>

#include "fuzz.hpp"

bool operator!=(const ramfuzz::harness<A> &a, const ramfuzz::harness<A> &b) {
  if (!a && !b)
    return false;
  if (bool(a) != bool(b))
    return true;
  return a.obj != b.obj;
}

int main() {
  using namespace ramfuzz::runtime;
  using namespace std;
  unique_ptr<gen> g(new gen("fuzzlog1"));
  auto rf1 = spin_roulette<ramfuzz::harness<A>>(*g);
  g.reset(new gen("fuzzlog1", "fuzzlog2"));
  auto rf2 = spin_roulette<ramfuzz::harness<A>>(*g);
  return rf1 != rf2;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
