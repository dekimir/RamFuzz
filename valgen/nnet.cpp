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

#include <torch/torch.h>

#include "dataset.hpp"

using namespace std;
using namespace ramfuzz;

namespace ramfuzz {

valgen_nnet::valgen_nnet() : Module("ValgenNet"), lin(nullptr) {
  // Register all parameters:
  // batchnorm
  // embedding
  // all conv1d
  // both linear
  lin = register_module("lin1", torch::nn::Linear(10, 2));

  // Need as large a range as possible for input values, which come from
  // arbitrary C++ programs:
  to(at::kDouble);
}

torch::Tensor valgen_nnet::forward(torch::Tensor vals, torch::Tensor locs) {
  // values > batchnorm
  // ids > embedding
  // concat
  // dropout
  // for each filtsz:
  //   flatten(maxpooling(conv1d))
  // linear(linear(dropout(concat(conv_list))))
  return torch::softmax(lin->forward(vals), 0);
}

void valgen_nnet::train_more(const exetree::node& root) {
  train();
  // Batch gradient descent implementation based on this suggestion:
  // https://discuss.pytorch.org/t/how-to-process-large-batches-of-data/6740/4
  auto opt = torch::optim::Adagrad(parameters(), 0.1);
  opt.zero_grad();
  for (exetree::dfs_cursor current(root); current; ++current) {
    const auto values = last_n(&*current, 10);
    const bool wins = current->dst()->maywin();
    const auto pred = forward(values, torch::zeros(1));
    const auto target = torch::tensor({wins, !wins}, at::kDouble);
    auto loss = torch::soft_margin_loss(pred, target);
    loss.backward();
  }
  opt.step();
}

}  // namespace ramfuzz
