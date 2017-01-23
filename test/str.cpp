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

#include "fuzz.hpp"

using namespace ramfuzz::runtime;
using namespace std;

vector<string> run(gen &g) {
  auto rf1 = spin_roulette<ramfuzz::harness<A>>(g);
  return rf1 ? rf1.obj.v : vector<string>();
}

int main() {
  vector<string> r1, r2;
  {
    gen g1("fuzzlog1");
    r1 = run(g1);
  }
  gen g2("fuzzlog1", "fuzzlog2");
  r2 = run(g2);
  return r1 != r2;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
