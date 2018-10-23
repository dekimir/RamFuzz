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

#include <gtest/gtest.h>
#include <iostream>
#include <memory>

#include "valgen.hpp"

using namespace ramfuzz;
using namespace std;

unique_ptr<valgen> global_valgen;

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  const auto seed = random_device{}();
  cout << "Using seed " << seed << " for valgen." << endl;
  global_valgen.reset(new valgen());
  return RUN_ALL_TESTS();
}
