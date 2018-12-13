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
#include <torch/script.h>
#include <torch/torch.h>
#include <limits>
#include <random>
#include <vector>

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

void extract_parameters(const torch::jit::script::Module& mod,
                        vector<torch::Tensor>& v) {
  for (const auto& p : mod.get_parameters()) v.push_back(*p.value().slot());
  for (const auto& m : mod.get_modules())
    extract_parameters(*m.value().module, v);
}

vector<torch::Tensor> extract_parameters(
    const torch::jit::script::Module& mod) {
  vector<torch::Tensor> params;
  extract_parameters(mod, params);
  return params;
}

vector<c10::IValue> last_n2(const edge* e, size_t n) {
  vector<c10::IValue> values(n);
  auto edge_ptr = e;
  auto i = n;
  while (i > 0 && edge_ptr) {
    --i;
    values[i] = double(*edge_ptr);
    edge_ptr = edge_ptr->src()->incoming_edge();
  }
  if (i == 0) return values;
  vector<c10::IValue> shifted(values.cbegin() + i, values.cend());
  while (i > 0) shifted.push_back(0.);
  return shifted;
}

void tmor(torch::jit::script::Module& net, const node& root) {
  auto opt = torch::optim::Adagrad(extract_parameters(net), 0.04);
  for (exetree::dfs_cursor current(root); current; ++current) {
    const auto pred = net.forward(last_n2(&*current, 10));
    const auto target = net.bool_as_prediction(current->dst()->maywin());
    torch::soft_margin_loss(pred, target).backward();
  }
}

TEST(ScriptModule, One) {
  auto m = torch::jit::load("delete_me.pt");
  node root;
  for (int i = -1000; i <= 1000; ++i) {
    root.find_or_add_edge(i)->maywin(i >= 0);
    if (!i % 20) tmor(*m, root);
  }
}

}  // namespace
