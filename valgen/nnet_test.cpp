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

#include "dataset.hpp"
#include "util.hpp"

namespace {

using namespace ramfuzz;
using namespace exetree;

class NNetTest : public testing::Test {
 protected:
  torch::Tensor pred(const torch::Tensor& input) {
    const auto dummy_locs = torch::tensor({1.});  // Currently ignored.
    return nn.forward(input, dummy_locs);
  }
  valgen_nnet nn;
  node root;
  const torch::Tensor success = torch::tensor({1., 0.});
  const torch::Tensor failure = torch::tensor({0., 1.});
};

#define EXPECT_PREDICTION(target, input)                          \
  {                                                               \
    const auto output = pred(input);                              \
    EXPECT_TRUE(torch::allclose((target), (output))) << (output); \
  }

/// Tests that valgen_nnet can learn a simple "negative values fail" case.
TEST_F(NNetTest, EasySplit) {
  for (int i = -1000; i <= 1000; ++i) root.find_or_add_edge(i)->maywin(i >= 0);
  for (int i = 0; i < 10; ++i) nn.train_more(root);
  EXPECT_PREDICTION(success, pad_right({100.}));
}

}  // namespace
