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

#include "dataset.hpp"

#include <gtest/gtest.h>
#include <torch/data/dataloader.h>
#include <torch/data/datasets.h>
#include <torch/data/samplers.h>
#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include "util.hpp"

namespace {

using namespace std;
using namespace ramfuzz;

class DatasetTest : public ::testing::Test {
 protected:
  /// Load exetree data into result.
  void load() {
    for (exetree::dfs_cursor current(root); current; ++current)
      result.emplace_back(last_n(&*current, 10),
                          torch::tensor(current->dst()->maywin()));
  }

  /// Zeros tensor in the shape of expected result data.
  torch::Tensor zeros() const { return torch::zeros(10, at::kDouble); }

  exetree::node root;

  vector<torch::data::Example<>> result;  ///< Holds load() result.
};

#define EXPECT_RESULT(i, expdata, exptarget)                                \
  {                                                                         \
    EXPECT_TRUE(torch::equal((expdata), result[i].data))                    \
        << "data[" << (i) << "]: " << result[i].data;                       \
    EXPECT_TRUE(torch::equal(torch::tensor({exptarget}), result[i].target)) \
        << "target[" << (i) << "]: " << result[i].target;                   \
  }

TEST_F(DatasetTest, SingleEdge) {
  root.find_or_add_edge(123.)->maywin(true);
  load();
  EXPECT_RESULT(0, pad_right({123.}), 1);
  EXPECT_EQ(1, result.size());
}

TEST_F(DatasetTest, ShortLinear) {
  root.find_or_add_edge(1.)
      ->find_or_add_edge(2.)
      ->find_or_add_edge(3.)
      ->find_or_add_edge(4.);
  load();
  EXPECT_EQ(4, result.size());
  auto exp = zeros();
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j <= i; ++j) exp[j] = j + 1;
    EXPECT_RESULT(i, exp, 0);
  }
}

TEST_F(DatasetTest, LongLinear) {
  root.find_or_add_edge(1.)
      ->find_or_add_edge(2.)
      ->find_or_add_edge(3.)
      ->find_or_add_edge(4.)
      ->find_or_add_edge(5.)
      ->find_or_add_edge(6.)
      ->find_or_add_edge(7.)
      ->find_or_add_edge(8.)
      ->find_or_add_edge(9.)
      ->find_or_add_edge(10.)
      ->find_or_add_edge(11.)
      ->find_or_add_edge(12.)
      ->find_or_add_edge(13.);
  load();
  EXPECT_EQ(13, result.size());
  auto exp = zeros();
  for (int i = 0; i < 13; ++i) {
    for (int j = 0; j < min(i + 1, 10); ++j) exp[j] = j + 1 + max(0, i - 9);
    EXPECT_RESULT(i, exp, 0);
  }
}

TEST_F(DatasetTest, Bushy) {
  // root > n1 > n2
  //      > n3 > n4
  //           > n5 > n6
  root.find_or_add_edge(1.)->find_or_add_edge(2.);
  const auto n3 = root.find_or_add_edge(3.);
  n3->find_or_add_edge(4.)->maywin(true);
  n3->maywin(true);
  root.maywin(true);
  n3->find_or_add_edge(5.)->find_or_add_edge(6.);
  load();
  EXPECT_EQ(6, result.size());
  EXPECT_RESULT(0, pad_right({1.}), 0);          // n1
  EXPECT_RESULT(1, pad_right({1., 2.}), 0);      // n2
  EXPECT_RESULT(2, pad_right({3.}), 1);          // n3
  EXPECT_RESULT(3, pad_right({3., 4.}), 1);      // n4
  EXPECT_RESULT(4, pad_right({3., 5.}), 0);      // n5
  EXPECT_RESULT(5, pad_right({3., 5., 6.}), 0);  // n6
}

}  // namespace
