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

TEST(Dataset, SingleEdge) {
  exetree::node n;
  n.find_or_add_edge(123.)->maywin(true);
  const auto loader = exetree::make_data_loader(n);
  auto i = 0u;
  torch::data::Example<> expected(torch::zeros(10, at::kDouble),
                                  torch::tensor({1}));
  expected.data[0] = 123.;
  for (auto batch : *loader)
    for (auto sample : batch) {
      EXPECT_TRUE(torch::equal(expected.data, sample.data))
          << i << ' ' << sample.data;
      EXPECT_TRUE(torch::equal(expected.target, sample.target))
          << i << ' ' << sample.target;
      i++;
    }
  EXPECT_EQ(1, i);
}

TEST(Dataset, DeepLinear) {
  exetree::node root;
  const auto n1 = root.find_or_add_edge(1.), n2 = n1->find_or_add_edge(2.),
             n3 = n2->find_or_add_edge(3.), n4 = n3->find_or_add_edge(4.);
  const auto loader = exetree::make_data_loader(root);
  vector<torch::Tensor> data, labels;
  for (auto batch : *loader)
    for (auto ex : batch) {
      data.push_back(ex.data);
      labels.push_back(ex.target);
    }
  EXPECT_EQ(4, data.size());
  EXPECT_EQ(4, labels.size());
  EXPECT_TRUE(torch::equal(
      torch::tensor({1., 0., 0., 0., 0., 0., 0., 0., 0., 0.}), data[0]))
      << data[0];
  EXPECT_TRUE(torch::equal(torch::tensor({0}), labels[0])) << labels[0];
  EXPECT_TRUE(torch::equal(
      torch::tensor({1., 2., 0., 0., 0., 0., 0., 0., 0., 0.}), data[1]))
      << data[1];
  EXPECT_TRUE(torch::equal(torch::tensor({0}), labels[0])) << labels[1];
  EXPECT_TRUE(torch::equal(
      torch::tensor({1., 2., 3., 0., 0., 0., 0., 0., 0., 0.}), data[2]))
      << data[2];
  EXPECT_TRUE(torch::equal(torch::tensor({0}), labels[0])) << labels[2];
  EXPECT_TRUE(torch::equal(
      torch::tensor({1., 2., 3., 4., 0., 0., 0., 0., 0., 0.}), data[3]))
      << data[3];
  EXPECT_TRUE(torch::equal(torch::tensor({0}), labels[0])) << labels[3];
}

}  // namespace
