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
#include <torch/script.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>

#include "valgen.hpp"

using namespace ramfuzz;
using namespace std;

unique_ptr<valgen> global_valgen;
unique_ptr<mt19937> global_testrng;
unsigned test_seed = random_device{}();

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  if (argc > 1) {
    if (string("--seed") != argv[1]) {
      cerr << "Unknown option: '" << argv[1] << "'\n";
      cerr << "usage: " << argv[0] << " [--seed <number>]" << endl;
      exit(11);
    }
    if (argc < 3) {
      cerr << "--seed requires a value" << endl;
      exit(22);
    }
    test_seed = atoi(argv[2]);
  }
  cout << "Using seed " << test_seed << " for valgen and randomized tests."
       << endl;
  global_valgen.reset(new valgen(test_seed));
  global_testrng.reset(new mt19937(test_seed));
  return RUN_ALL_TESTS();
}
