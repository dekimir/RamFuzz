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

/// Tests that valgen_nnet can learn a simple case of "all values above this
/// threshold fail".
TEST_F(NNetTest, EasySplit) {
  mt19937 rneng;
  uniform_int_distribution<int> dist(numeric_limits<int>::min(),
                                     numeric_limits<int>::max());
  // Explicitly add some values, to ensure the net trains on them.  This is
  // realistic: it tests valgen will not make repeated mistakes, even if it
  // misclassifies a value the first time it's generated.
  root.find_or_add_edge(100)->maywin(true);
  root.find_or_add_edge(300)->maywin(true);
  for (int i = 0; i <= 4000; ++i) {
    const auto v = dist(rneng);
    root.find_or_add_edge(v)->maywin(v < 1000);
    if (i % 40 == 0) nn.train_more(root);
  }
  EXPECT_PREDICTION(success, pad_right({-10000.}));
  EXPECT_PREDICTION(success, pad_right({-1000.}));
  EXPECT_PREDICTION(success, pad_right({-100.}));
  EXPECT_PREDICTION(success, pad_right({100.}));
  EXPECT_PREDICTION(success, pad_right({300.}));
  EXPECT_PREDICTION(failure, pad_right({1000.}));
  EXPECT_PREDICTION(failure, pad_right({2000.}));
  EXPECT_PREDICTION(failure, pad_right({5000.}));
  EXPECT_PREDICTION(failure, pad_right({10000.}));
}

TEST_F(NNetTest, NoFailuresEver) {
  mt19937 eng;
  uniform_int_distribution<int> dist(numeric_limits<int>::min(),
                                     numeric_limits<int>::max());
  auto* cur = &root;
  for (int i = 0; i < 1000; ++i) {
    cur->maywin(true);
    cur = cur->find_or_add_edge(dist(eng));
    if (i % 40 == 0) nn.train_more(root);
  }
  cur->terminal(node::SUCCESS);
  cur->maywin(true);
  size_t succ_count = 0;
  for (dfs_cursor iter(root); iter; ++iter)
    succ_count += (nn.predict(last_n(&*iter, 10)) == iter->dst()->maywin());
  EXPECT_GE(succ_count, 850);
}

}  // namespace
