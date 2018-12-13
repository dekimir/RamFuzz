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

#include "nnet.hpp"

#include <gtest/gtest.h>
#include <torch/torch.h>
#include <limits>
#include <random>

#include "dataset.hpp"
#include "util.hpp"

namespace {

using namespace std;
using namespace ramfuzz;
using namespace exetree;

class NNetTest : public testing::Test {
 protected:
  valgen_nnet nn;
  node root;
};

constexpr bool success = true, failure = false;

#define EXPECT_PREDICTION(expected, input)                \
  {                                                       \
    const auto p = nn.forward(input);                     \
    EXPECT_EQ((expected), nn.prediction_as_bool(p)) << p; \
  }

/// Tests that valgen_nnet can learn a simple "negative values fail" case.
TEST_F(NNetTest, EasySplit) {
  for (int i = -1000; i <= 1000; ++i) {
    root.find_or_add_edge(i)->maywin(i >= 0);
    if (!i % 20) nn.train_more(root);
  }

  EXPECT_PREDICTION(success, pad_right({100.}));
  EXPECT_PREDICTION(success, pad_right({1000.}));
  EXPECT_PREDICTION(success, pad_right({10000.}));

  EXPECT_PREDICTION(failure, pad_right({-100.}));
  EXPECT_PREDICTION(failure, pad_right({-1000.}));
  EXPECT_PREDICTION(failure, pad_right({-10000.}));
}

}  // namespace
