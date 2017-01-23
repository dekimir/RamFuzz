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

/// Tests that correct code is generated when the class under test has no public
/// constructors.

using namespace ramfuzz;

int main(int argc, char *argv[]) {
  runtime::gen g(argc, argv);
  return g.make<C>()->id != 234;
}

harness<B>::harness(runtime::gen &g) : g(g), pobj(B::create()), obj(*pobj) {}

unsigned runtime::spinlimit = 3;
