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
#include <memory>
#include <vector>

namespace {

using namespace std;
using namespace ramfuzz;

class TestDataset : public torch::data::datasets::Dataset<TestDataset> {
 public:
  TestDataset() : iseen(new vector<size_t>) {}
  ExampleType get(size_t index) override {
    iseen->push_back(index);
    return {torch::tensor({12, int(index)}), torch::tensor({34, int(index)})};
  }
  torch::optional<size_t> size() const override { return 100; }
  shared_ptr<vector<size_t>> iseen;
};

static constexpr size_t batch_size = 10;

/// Ensures the torch DataLoader behaves the way we expect and depend on.
TEST(DataLoader, Order) {
  TestDataset ds;
  auto loader = torch::data::make_data_loader(
      ds, batch_size, torch::data::samplers::SequentialSampler(100));
  int i = 0;
  for (auto batch : *loader)
    for (auto ex : batch) {
      EXPECT_TRUE(torch::equal(torch::tensor({12, i}), ex.data))
          << i << ' ' << ex.data;
      EXPECT_TRUE(torch::equal(torch::tensor({34, i}), ex.target))
          << i << ' ' << ex.target;
      EXPECT_EQ(i, ds.iseen->at(i));
      i++;
    }
}

class DatasetTest : public ::testing::Test {
 protected:
  /// Run exetree data loader on root and record the result.
  void load() {
    const auto loader = exetree::make_data_loader(root);
    for (auto batch : *loader)
      for (auto ex : batch) result.push_back(ex);
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
  auto exp = zeros();
  exp[0] = 123.;
  EXPECT_RESULT(0, exp, 1);
  EXPECT_EQ(1, result.size());
}

TEST_F(DatasetTest, ShortLinear) {
  root.find_or_add_edge(1.)
      ->find_or_add_edge(2.)
      ->find_or_add_edge(3.)
      ->find_or_add_edge(4.);
  load();
  EXPECT_EQ(4, result.size());
  auto exp = torch::zeros(10, at::kDouble);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j <= i; ++j) exp[j] = double(j + 1);
    EXPECT_RESULT(i, exp, 0);
  }
}

}  // namespace
